/*
 * JBC C245 Soldering Station Controller - 2 kanały
 *
 * Hardware:
 *   - Raspberry Pi Pico (RP2040)
 *   - 2x MAX6675 (termopara K, moduły HW-550)
 *   - 2x N-MOSFET PWM (grzałki)
 *   - Nextion NX3224T028 / K028 (320x240, dotykowy)
 *   - Dzielnik napięcia wejściowego (33k / 4.7k)
 *   - 3x przyciski z podświetleniem (szybkie profile)
 *   - 2x czujnik odłożenia kolby (zwarcie do GND)
 *   - styk wymiany grota (kolba 1)
 *
 * Autor: Dzejko
 */

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "config_io.h"
#include "tempsense.h"
#include "pid.h"
#include "channel.h"
#include "buttons.h"
#include "nextion.h"
#include "ui.h"

#define VOLTAGE_UPDATE_MS   500
#define WATCHDOG_MS         2000

SystemState_t g_sys;
static uint32_t g_loop_max_us = 0;     // najdłuższy obieg pętli od ostatniego "nx"

// ============================================================
//  Dioda na płytce Pico (GP25) - znak życia programu
//    wolne miganie (1 Hz)  : program działa, brak błędów
//    szybkie miganie (4 Hz): przegrzanie albo błąd grzania
//    nie miga              : program stoi (watchdog zaraz zrestartuje)
// ============================================================
#ifdef PICO_DEFAULT_LED_PIN
#define PIN_STATUS_LED  PICO_DEFAULT_LED_PIN
#endif

static void status_led_init(void) {
#ifdef PIN_STATUS_LED
    gpio_init(PIN_STATUS_LED);
    gpio_set_dir(PIN_STATUS_LED, GPIO_OUT);
    gpio_put(PIN_STATUS_LED, 0);
#endif
}

static void status_led_update(uint32_t now_ms) {
#ifdef PIN_STATUS_LED
    bool fault = false;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        Fault_t f = g_sys.ch[i].fault;
        if (f == FAULT_OVERTEMP || f == FAULT_RUNAWAY) fault = true;
    }
    uint32_t half_period = fault ? 125 : 500;
    gpio_put(PIN_STATUS_LED, ((now_ms / half_period) & 1) != 0);
#else
    (void)now_ms;
#endif
}

// ============================================================
//  Licznik czasu grzania (strona "system", t_hours)
//  Liczy sekundy, w których kanał może grzać (PRACA / CZUWANIE / HIBERNACJA).
//  Zapis do flash co WORK_SAVE_PERIOD_S naliczonego czasu, żeby oszczędzać pamięć.
// ============================================================
static void work_counter_update(uint32_t now_ms) {
    static uint32_t last_ms = 0;
    static uint32_t unsaved_s = 0;
    if (last_ms == 0) { last_ms = now_ms; return; }
    if ((now_ms - last_ms) < 1000) return;
    last_ms += 1000;

    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (channel_is_heating_allowed(&g_sys.ch[i])) {
            g_sys.config.ch[i].work_s++;
            unsaved_s++;
        }
    }
    if (unsaved_s >= WORK_SAVE_PERIOD_S) {
        unsaved_s = 0;
        channels_store_to_config(&g_sys);
        config_save(&g_sys.config);
        g_sys.config_dirty = 0;
    }
}

// ============================================================
//  Przyciski fizyczne
//   krótko  : profil 1/2/3 dla wybranej kolby
//   długo P1: przełącz wybraną kolbę (1 <-> 2)
//   długo P3: włącz / wyłącz wybraną kolbę
// ============================================================
static void handle_physical_buttons(uint32_t now_ms) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        ButtonEvent_t ev = buttons_poll(i, now_ms);
        if (ev == BTN_EV_NONE) continue;

        Channel_t *sel = &g_sys.ch[g_sys.selected_ch];
        if (ev == BTN_EV_SHORT) {
            channel_set_profile(sel, &g_sys.config, i);
            ui_mark_dirty(&g_sys, now_ms);
            printf("[BTN] kolba %d: profil %d = %d C\n",
                   g_sys.selected_ch + 1, i + 1, g_sys.config.profile_temp[i]);
        } else if (ev == BTN_EV_LONG) {
            if (i == 0 && NUM_CHANNELS > 1) {
                ui_select_channel(&g_sys, (g_sys.selected_ch + 1) % NUM_CHANNELS, now_ms);
                printf("[BTN] wybrana kolba %d\n", g_sys.selected_ch + 1);
            } else if (i == 2) {
                channel_toggle_power(sel);
                printf("[BTN] kolba %d: %s\n", g_sys.selected_ch + 1,
                       sel->mode == MODE_OFF ? "WYL" : "WL");
            }
        }
    }
}

// ============================================================
//  Konsola USB (115200, dowolny terminal) - strojenie i diagnostyka
// ============================================================
static const char *mode_short(const Channel_t *ch) {
    if (ch->fault == FAULT_NO_TIP)   return "NOTIP";
    if (ch->fault == FAULT_OVERTEMP) return "OVERT";
    if (ch->fault == FAULT_RUNAWAY)  return "RUNAW";
    switch (ch->mode) {
        case MODE_ACTIVE:    return "ACT";
        case MODE_STANDBY:   return "SBY";
        case MODE_HIBERNATE: return "HIB";
        case MODE_EXCHANGE:  return "EXC";
        default:             return "OFF";
    }
}

static void print_status(void) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
        const Channel_t *ch = &g_sys.ch[i];
        printf("CH%d: %5.1f/%3d C  PID=%3d%%  [%s] dock=%d tip=%d  Kp=%.1f Ki=%.2f Kd=%.1f off=%+d\n",
               i + 1, ch->temp_current, channel_target_temp(ch, &g_sys.config),
               (int)ch->power_percent, mode_short(ch), channel_is_docked(i), ch->tip_change_active,
               ch->pid.kp, ch->pid.ki, ch->pid.kd, g_sys.config.ch[i].temp_offset);
    }
    printf("Vin=%.1fV  wybrana=%d  nextion=%s\n", g_sys.v_input, g_sys.selected_ch + 1,
           nextion_link_ok() ? "OK" : "brak odp.");
    printf("Czuwanie->hibernacja %lus, hibernacja %d C, auto-off po %u min\n",
           (unsigned long)(g_sys.config.standby_delay_ms / 1000), g_sys.config.hibernate_temp,
           g_sys.config.auto_off_min);
    printf("Moc max=%d%%  krok=%d C  Tmax=%d C  jasnosc=%d%%  grzanie CH1=%lu s\n",
           g_sys.config.power_limit, g_sys.config.temp_step, g_sys.config.temp_max,
           g_sys.config.brightness, (unsigned long)g_sys.config.ch[0].work_s);
}

static void cli_execute(char *line, uint32_t now_ms) {
    char *cmd = strtok(line, " ");
    if (!cmd) return;

    if (!strcmp(cmd, "help")) {
        printf("status | log on|off | set <kanal> <temp> | pid <kanal> <kp> <ki> <kd>\n"
               "offset <kanal> <C> | on <kanal> | off <kanal> | aoff <min> | save | defaults | nx | tc | i2c\n");
    } else if (!strcmp(cmd, "status")) {
        print_status();
    } else if (!strcmp(cmd, "aoff")) {
        // Czas od początku hibernacji do wyłączenia grzania [min], 0 = nigdy
        char *a = strtok(NULL, " ");
        if (a) {
            g_sys.config.auto_off_min = (uint16_t)atoi(a);
            config_sanitize(&g_sys.config);
            ui_mark_dirty(&g_sys, now_ms);
        }
        printf("auto-wylaczenie po %u min hibernacji (0 = nigdy)\n", g_sys.config.auto_off_min);
    } else if (!strcmp(cmd, "i2c")) {
        tsens_i2c_scan();
    } else if (!strcmp(cmd, "tc")) {
        tsens_debug_print();
    } else if (!strcmp(cmd, "nx")) {
        nextion_debug_print();
        printf("Najdluzszy obieg petli: %lu us\n", (unsigned long)g_loop_max_us);
        g_loop_max_us = 0;
    } else if (!strcmp(cmd, "log")) {
        char *a = strtok(NULL, " ");
        g_sys.log_enabled = (a && !strcmp(a, "on"));
        if (g_sys.log_enabled) printf("ms,ch,temp,target,duty,mode\n");
    } else if (!strcmp(cmd, "save")) {
        ui_save_now(&g_sys, now_ms);
    } else if (!strcmp(cmd, "defaults")) {
        uint32_t work[NUM_CHANNELS];     // liczniki godzin zostają
        for (int i = 0; i < NUM_CHANNELS; i++) work[i] = g_sys.config.ch[i].work_s;
        config_set_defaults(&g_sys.config);
        for (int i = 0; i < NUM_CHANNELS; i++) g_sys.config.ch[i].work_s = work[i];
        for (int i = 0; i < NUM_CHANNELS; i++) channel_init(&g_sys.ch[i], i, &g_sys.config);
        ui_save_now(&g_sys, now_ms);
    } else {
        char *a = strtok(NULL, " ");
        int ch = a ? atoi(a) - 1 : -1;
        if (ch < 0 || ch >= NUM_CHANNELS) { printf("zly kanal (1-%d)\n", NUM_CHANNELS); return; }
        Channel_t *c = &g_sys.ch[ch];

        if (!strcmp(cmd, "set")) {
            char *t = strtok(NULL, " ");
            if (t) channel_adjust_setpoint(c, &g_sys.config, atoi(t) - (int)c->setpoint);
        } else if (!strcmp(cmd, "pid")) {
            char *p = strtok(NULL, " "), *i = strtok(NULL, " "), *d = strtok(NULL, " ");
            if (p && i && d) {
                c->pid.kp = strtof(p, NULL);
                c->pid.ki = strtof(i, NULL);
                c->pid.kd = strtof(d, NULL);
                pid_reset(&c->pid);
            }
        } else if (!strcmp(cmd, "offset")) {
            char *o = strtok(NULL, " ");
            if (o) g_sys.config.ch[ch].temp_offset = (int8_t)atoi(o);
            config_sanitize(&g_sys.config);
        } else if (!strcmp(cmd, "on")) {
            if (c->mode == MODE_OFF) channel_toggle_power(c); else channel_wake(c);
        } else if (!strcmp(cmd, "off")) {
            if (c->mode != MODE_OFF) channel_toggle_power(c);
        } else {
            printf("nieznana komenda - wpisz help\n");
            return;
        }
        ui_mark_dirty(&g_sys, now_ms);
        print_status();
    }
}

static void cli_poll(uint32_t now_ms) {
    static char line[64];
    static int  len = 0;
    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (c == '\r' || c == '\n') {
            if (len > 0) {
                line[len] = '\0';
                cli_execute(line, now_ms);
                len = 0;
            }
        } else if (len < (int)sizeof(line) - 1) {
            line[len++] = (char)c;
        }
    }
}

// ============================================================
//  MAIN
// ============================================================
int main(void) {
    // Grzałki wyłączone zanim cokolwiek innego się stanie
    channel_hw_init();

    stdio_init_all();
    printf("\n=== JBC C245 Station v" FW_VERSION " ===\n");
    if (watchdog_caused_reboot()) printf("!!! Restart przez watchdog !!!\n");

    config_load(&g_sys.config);
    g_sys.selected_ch = g_sys.config.selected_ch;
    printf("Profile %d/%d/%d C, czuwanie %lus, hibernacja %d C, auto-off %d min\n",
           g_sys.config.profile_temp[0], g_sys.config.profile_temp[1],
           g_sys.config.profile_temp[2], (unsigned long)(g_sys.config.standby_delay_ms / 1000),
           g_sys.config.hibernate_temp, g_sys.config.auto_off_min);

    buttons_init();
    adc_voltage_init();
    tsens_init();                   // po adc_init - korzysta z czujnika temperatury Pico
    for (int i = 0; i < NUM_CHANNELS; i++) {
        channel_init(&g_sys.ch[i], i, &g_sys.config);
    }
    g_sys.v_input = adc_read_voltage();

    ui_init(&g_sys);

    status_led_init();
    watchdog_enable(WATCHDOG_MS, true);
    printf("Start petli sterowania (wpisz 'help')\n");

    uint32_t last_voltage_ms = 0;

    uint32_t loop_start_us = time_us_32();
    while (true) {
        uint32_t loop_now_us = time_us_32();
        if (loop_now_us - loop_start_us > g_loop_max_us) g_loop_max_us = loop_now_us - loop_start_us;
        loop_start_us = loop_now_us;
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        if (control_tick(&g_sys, now_ms) && g_sys.log_enabled) {
            for (int i = 0; i < NUM_CHANNELS; i++) {
                const Channel_t *ch = &g_sys.ch[i];
                printf("%lu,%d,%.2f,%d,%d,%s\n", (unsigned long)now_ms, i + 1, ch->temp_current,
                       channel_target_temp(ch, &g_sys.config), ch->pwm_duty, mode_short(ch));
            }
        }

        if ((now_ms - last_voltage_ms) >= VOLTAGE_UPDATE_MS) {
            last_voltage_ms = now_ms;
            g_sys.v_input = 0.7f * g_sys.v_input + 0.3f * adc_read_voltage();
        }

        work_counter_update(now_ms);
        status_led_update(now_ms);
        handle_physical_buttons(now_ms);
        button_lights_update(&g_sys, now_ms);
        ui_task(&g_sys, now_ms);
        cli_poll(now_ms);

        watchdog_update();
    }
}
