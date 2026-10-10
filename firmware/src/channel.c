#include "channel.h"
#include "pid.h"
#include "tempsense.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include <math.h>

static const uint PWM_PINS[HW_CHANNELS]    = {PIN_PWM_CH1,     PIN_PWM_CH2};
static const uint DOCK_PINS[HW_CHANNELS]   = {PIN_STAT_IN1,    PIN_STAT_IN2};
static const uint TIP_PINS[HW_CHANNELS]    = {PIN_TIP_CHANGE1, PIN_TIP_CHANGE2};

static uint pwm_slice[NUM_CHANNELS];
static uint pwm_chan[NUM_CHANNELS];

#define EMA_ALPHA   0.5f    // pomiar co ~0.6 s - lekki filtr wystarcza

// ============================================================
//  Sprzęt
// ============================================================
void channel_hw_init(void) {
    // PWM: clk_sys / div = 1 MHz, wrap = 1000 -> 1 kHz
    float div = (float)clock_get_hz(clk_sys) / (PWM_FREQ_HZ * (float)PWM_MAX_DUTY);

    // Wszystkie bramki MOSFET (także nieobsługiwanych kolb) trzymane w stanie niskim
    for (int i = 0; i < HW_CHANNELS; i++) {
        gpio_init(PWM_PINS[i]);
        gpio_set_dir(PWM_PINS[i], GPIO_OUT);
        gpio_put(PWM_PINS[i], 0);
    }

    for (int i = 0; i < NUM_CHANNELS; i++) {
        pwm_slice[i] = pwm_gpio_to_slice_num(PWM_PINS[i]);
        pwm_chan[i]  = pwm_gpio_to_channel(PWM_PINS[i]);
        pwm_set_clkdiv(pwm_slice[i], div);
        pwm_set_wrap(pwm_slice[i], PWM_MAX_DUTY - 1);   // level = 1000 => 100%
        pwm_set_chan_level(pwm_slice[i], pwm_chan[i], 0);
        pwm_set_enabled(pwm_slice[i], true);
        gpio_set_function(PWM_PINS[i], GPIO_FUNC_PWM);
    }

    for (int i = 0; i < NUM_CHANNELS; i++) {
        gpio_init(DOCK_PINS[i]);
        gpio_set_dir(DOCK_PINS[i], GPIO_IN);
        gpio_pull_up(DOCK_PINS[i]);

        if (TIP_PINS[i] != PIN_NONE) {
            gpio_init(TIP_PINS[i]);
            gpio_set_dir(TIP_PINS[i], GPIO_IN);
            gpio_pull_up(TIP_PINS[i]);
        }
    }
}

static void set_pwm_duty(int ch, uint16_t duty) {
    if (duty > PWM_MAX_DUTY) duty = PWM_MAX_DUTY;
    pwm_set_chan_level(pwm_slice[ch], pwm_chan[ch], duty);
}

void heaters_all_off(void) {
    for (int i = 0; i < NUM_CHANNELS; i++) set_pwm_duty(i, 0);
}

static uint8_t pin_is_active(uint pin, uint8_t active_low) {
    if (pin == PIN_NONE) return 0;
    uint8_t level = gpio_get(pin) ? 1 : 0;
    return active_low ? !level : level;
}

uint8_t channel_is_docked(int ch) {
    return pin_is_active(DOCK_PINS[ch], STAT_IN_ACTIVE_LOW);
}

uint8_t channel_tip_change_active(int ch) {
    return pin_is_active(TIP_PINS[ch], TIP_CHANGE_ACTIVE_LOW);
}

// ============================================================
//  Inicjalizacja kanału
// ============================================================
void channel_apply_pid_gains(Channel_t *ch, const ChannelConfig_t *cc) {
    ch->pid.kp = cc->kp;
    ch->pid.ki = cc->ki;
    ch->pid.kd = cc->kd;
}

void channel_init(Channel_t *ch, int idx, const Config_t *cfg) {
    const ChannelConfig_t *cc = &cfg->ch[idx];
    *ch = (Channel_t){0};
    ch->idx            = (uint8_t)idx;
    ch->setpoint       = cc->setpoint;
    ch->active_profile = cc->active_profile;
    ch->mode           = MODE_ACTIVE;
    ch->fault          = FAULT_NONE;
    pid_init(&ch->pid, cc->kp, cc->ki, cc->kd);
}

bool channel_is_heating_allowed(const Channel_t *ch) {
    return ch->fault == FAULT_NONE &&
           !ch->tip_change_active &&
           (ch->mode == MODE_ACTIVE || ch->mode == MODE_STANDBY || ch->mode == MODE_HIBERNATE);
}

// ============================================================
//  Logika czuwania
//   odłożona -> STANDBY (trzyma nastawę) -> po standby_delay HIBERNATE
//   -> po auto_off_min OFF.  Podniesienie budzi natychmiast.
// ============================================================
static void channel_update_dock(Channel_t *ch, const Config_t *cfg, uint32_t now_ms) {
    uint8_t raw_docked = channel_is_docked(ch->idx);

    // Podniesienie działa natychmiast, bez debounce
    if (!raw_docked) {
        ch->dock_raw_state = 0;
        if (ch->iron_docked) {
            ch->iron_docked = 0;
            bool auto_off = (ch->mode == MODE_OFF && !ch->off_by_user);
            if (ch->mode == MODE_STANDBY || ch->mode == MODE_HIBERNATE || auto_off) {
                ch->mode = MODE_ACTIVE;
                pid_reset(&ch->pid);
            }
        }
        return;
    }

    // Odłożenie filtrujemy czasowo
    if (!ch->dock_raw_state) {
        ch->dock_raw_state    = 1;
        ch->dock_raw_since_ms = now_ms;
        return;
    }
    if ((now_ms - ch->dock_raw_since_ms) < STANDBY_DEBOUNCE_MS) return;

    ch->iron_docked = 1;

    // ACTIVE przy odłożonej kolbie (świeżo odłożona albo wybudzona przyciskiem)
    if (ch->mode == MODE_ACTIVE) {
        ch->mode            = MODE_STANDBY;
        ch->docked_since_ms = now_ms;
    }

    if (ch->mode == MODE_STANDBY &&
        (now_ms - ch->docked_since_ms) >= cfg->standby_delay_ms) {
        ch->mode               = MODE_HIBERNATE;
        ch->hibernate_since_ms = now_ms;
        pid_reset(&ch->pid);
    }

    if (ch->mode == MODE_HIBERNATE && cfg->auto_off_min > 0 &&
        (now_ms - ch->hibernate_since_ms) >= (uint32_t)cfg->auto_off_min * 60000u) {
        ch->mode        = MODE_OFF;
        ch->off_by_user = 0;
    }
}

// ============================================================
//  Przetworzenie jednego pomiaru: tryby, zabezpieczenia, PID
// ============================================================
static void channel_process(Channel_t *ch, const Config_t *cfg,
                            float raw_temp, uint8_t tc_err,
                            uint32_t now_ms, float dt) {
    // --- Styk wymiany grota ---
    ch->tip_change_active = channel_tip_change_active(ch->idx);
    if (ch->tip_change_active && !ch->tip_change_prev) {
        ch->mode = MODE_EXCHANGE;
        ch->tip_change_block_until_ms = now_ms + TIP_CHANGE_BLOCK_MS;
        pid_reset(&ch->pid);
    }
    ch->tip_change_prev = ch->tip_change_active;

    if (ch->mode == MODE_EXCHANGE &&
        (int32_t)(now_ms - ch->tip_change_block_until_ms) >= 0 &&
        !ch->tip_change_active && !tc_err) {
        ch->mode = MODE_ACTIVE;
        pid_reset(&ch->pid);
    }

    // --- Temperatura ---
    if (tc_err) {
        // Brak grota ma pierwszeństwo przed BLAD GRZANIA - inaczej ekran pokazywałby
        // ostatnią zapamiętaną temperaturę. PRZEGRZANIE zostaje zablokowane.
        if (ch->fault == FAULT_NONE || ch->fault == FAULT_RUNAWAY) ch->fault = FAULT_NO_TIP;
        ch->filter_init = 0;
    } else {
        if (ch->fault == FAULT_NO_TIP) {
            ch->fault = FAULT_NONE;     // kolba/grot wrócił
            pid_reset(&ch->pid);
        }
        float t = raw_temp + (float)cfg->ch[ch->idx].temp_offset;
        if (!ch->filter_init) {
            ch->temp_current = t;
            ch->filter_init  = 1;
        } else {
            ch->temp_current = EMA_ALPHA * t + (1.0f - EMA_ALPHA) * ch->temp_current;
        }
        if (t > TEMP_OVERHEAT_C) ch->fault = FAULT_OVERTEMP;
    }

    // --- Czuwanie ---
    if (ch->mode != MODE_EXCHANGE) {
        channel_update_dock(ch, cfg, now_ms);
    }

    // --- PID ---
    // Limit mocy z ustawień: ogranicza wyjście regulatora (anti-windup liczy się z nim)
    uint16_t duty_limit = (uint16_t)((uint32_t)PWM_MAX_DUTY * cfg->power_limit / 100u);
    ch->pid.output_limit = (float)duty_limit;

    uint16_t duty = 0;
    if (channel_is_heating_allowed(ch)) {
        float target = (float)channel_target_temp(ch, cfg);
        duty = (uint16_t)(pid_compute(&ch->pid, target, ch->temp_current, dt) + 0.5f);
    } else {
        pid_reset(&ch->pid);
    }

    // --- Ochrona: długo pełna moc, a temperatura stoi ---
    if (duty > 0 && (uint32_t)duty * 100u >= (uint32_t)duty_limit * RUNAWAY_DUTY_PCT) {
        if (!ch->sat_tracking) {
            ch->sat_tracking   = 1;
            ch->sat_since_ms   = now_ms;
            ch->sat_start_temp = ch->temp_current;
        } else if ((now_ms - ch->sat_since_ms) >= RUNAWAY_TIME_MS) {
            if ((ch->temp_current - ch->sat_start_temp) < RUNAWAY_MIN_RISE_C) {
                ch->fault = FAULT_RUNAWAY;
                duty = 0;
            }
            ch->sat_since_ms   = now_ms;
            ch->sat_start_temp = ch->temp_current;
        }
    } else {
        ch->sat_tracking = 0;
    }

    ch->pwm_duty      = duty;
    ch->power_percent = duty * 100.0f / PWM_MAX_DUTY;
}

// ============================================================
//  Maszyna stanów: grzanie / cisza / pomiar
// ============================================================
typedef enum { PH_HEAT, PH_SETTLE, PH_CONVERT } Phase_t;

static Phase_t  g_phase         = PH_SETTLE;
static uint32_t g_phase_start   = 0;
static uint32_t g_last_meas_ms  = 0;

static void measure_and_regulate(SystemState_t *sys, uint32_t now_ms) {
    float dt = (g_last_meas_ms == 0) ? ((HEAT_WINDOW_MS + TC_SETTLE_MS + TSENS_CONV_MS) / 1000.0f)
                                     : (now_ms - g_last_meas_ms) / 1000.0f;
    g_last_meas_ms = now_ms;

    for (int i = 0; i < NUM_CHANNELS; i++) {
        uint8_t err = 0;
        float t = tsens_read(i, &err);
        channel_process(&sys->ch[i], &sys->config, t, err, now_ms, dt);
    }
}

bool control_tick(SystemState_t *sys, uint32_t now_ms) {
#if TC_SYNC_MEASURE
    uint32_t elapsed = now_ms - g_phase_start;
    switch (g_phase) {
        case PH_HEAT:
            if (elapsed >= HEAT_WINDOW_MS) {
                heaters_all_off();
                g_phase = PH_SETTLE;
                g_phase_start = now_ms;
            }
            break;
        case PH_SETTLE:
            if (elapsed >= TC_SETTLE_MS) {
                for (int i = 0; i < NUM_CHANNELS; i++) tsens_start(i);
                g_phase = PH_CONVERT;
                g_phase_start = now_ms;
            }
            break;
        case PH_CONVERT:
            if (elapsed >= TSENS_CONV_MS) {
                measure_and_regulate(sys, now_ms);
                for (int i = 0; i < NUM_CHANNELS; i++) set_pwm_duty(i, sys->ch[i].pwm_duty);
                g_phase = PH_HEAT;
                g_phase_start = now_ms;
                return true;
            }
            break;
    }
    return false;
#else
    if ((now_ms - g_phase_start) >= TC_READ_PERIOD_MS) {
        g_phase_start = now_ms;
        measure_and_regulate(sys, now_ms);
        for (int i = 0; i < NUM_CHANNELS; i++) set_pwm_duty(i, sys->ch[i].pwm_duty);
        return true;
    }
    return false;
#endif
}

// ============================================================
//  Akcje użytkownika
// ============================================================
void channel_wake(Channel_t *ch) {
    // Skasuj zablokowane błędy (przegrzanie tylko gdy już ostygło)
    if (ch->fault == FAULT_RUNAWAY) ch->fault = FAULT_NONE;
    if (ch->fault == FAULT_OVERTEMP && ch->temp_current < TEMP_OVERHEAT_C - 30) ch->fault = FAULT_NONE;
    ch->sat_tracking = 0;
    ch->off_by_user  = 0;

    if (ch->mode != MODE_ACTIVE && ch->mode != MODE_EXCHANGE) {
        ch->mode = MODE_ACTIVE;     // jeśli kolba w stojaku -> STANDBY z nowym licznikiem
        pid_reset(&ch->pid);
    }
}

void channel_toggle_power(Channel_t *ch) {
    if (ch->mode == MODE_OFF) {
        channel_wake(ch);
    } else {
        ch->mode        = MODE_OFF;
        ch->off_by_user = 1;
        ch->pwm_duty    = 0;
        set_pwm_duty(ch->idx, 0);
        pid_reset(&ch->pid);
    }
}

void channel_set_profile(Channel_t *ch, const Config_t *cfg, int profile_idx) {
    if (profile_idx < 0 || profile_idx >= NUM_PROFILES) return;
    ch->active_profile = (int8_t)profile_idx;
    ch->setpoint       = cfg->profile_temp[profile_idx];
    channel_wake(ch);
}

void channel_adjust_setpoint(Channel_t *ch, const Config_t *cfg, int delta) {
    int sp = (int)ch->setpoint + delta;
    if (sp < TEMP_SET_MIN) sp = TEMP_SET_MIN;
    if (sp > cfg->temp_max) sp = cfg->temp_max;
    ch->setpoint = (uint16_t)sp;

    ch->active_profile = -1;
    for (int p = 0; p < NUM_PROFILES; p++) {
        if (cfg->profile_temp[p] == ch->setpoint) ch->active_profile = (int8_t)p;
    }
    channel_wake(ch);
}

void channels_store_to_config(SystemState_t *sys) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
        sys->config.ch[i].setpoint       = sys->ch[i].setpoint;
        sys->config.ch[i].active_profile = sys->ch[i].active_profile;
        sys->config.ch[i].kp             = sys->ch[i].pid.kp;
        sys->config.ch[i].ki             = sys->ch[i].pid.ki;
        sys->config.ch[i].kd             = sys->ch[i].pid.kd;
    }
    sys->config.selected_ch = sys->selected_ch;
}
