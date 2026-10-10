#include "config.h"
#include "config_io.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stddef.h>

// Ostatni sektor flash - najdalej od kodu programu
#define FLASH_CONFIG_OFFSET  (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

_Static_assert(sizeof(Config_t) <= FLASH_PAGE_SIZE, "Config_t nie miesci sie w stronie flash");

// ============================================================
//  CRC32 - weryfikacja integralności danych w Flash
// ============================================================
static uint32_t crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

static uint32_t config_crc(const Config_t *cfg) {
    return crc32((const uint8_t *)cfg, offsetof(Config_t, crc));
}

// ============================================================
//  Domyślna konfiguracja
// ============================================================
void config_pid_defaults(ChannelConfig_t *cc) {
    cc->kp = PID_DEFAULT_KP;
    cc->ki = PID_DEFAULT_KI;
    cc->kd = PID_DEFAULT_KD;
}

void config_set_defaults(Config_t *cfg) {
    memset(cfg, 0, sizeof(Config_t));   // zeruje też padding (ważne dla CRC)
    cfg->magic = FLASH_CONFIG_MAGIC;

    cfg->profile_temp[0] = 300;
    cfg->profile_temp[1] = 360;
    cfg->profile_temp[2] = 450;

    for (int i = 0; i < NUM_CHANNELS; i++) {
        cfg->ch[i].setpoint       = cfg->profile_temp[0];
        cfg->ch[i].active_profile = 0;
        cfg->ch[i].temp_offset    = 0;
        config_pid_defaults(&cfg->ch[i]);
    }

    cfg->standby_delay_ms = 10000;
    cfg->hibernate_temp   = 150;
    cfg->auto_off_min     = 15;    // hibernacja -> grzanie wyłączone
    cfg->brightness       = 80;
    cfg->selected_ch      = 0;
    cfg->power_limit      = 100;
    cfg->temp_step        = TEMP_STEP_DEFAULT;
    cfg->temp_max         = 450;    // maks. temperatura robocza grotów C245
}

bool config_temp_step_valid(int step) {
    static const uint8_t opts[] = TEMP_STEP_OPTIONS;
    for (size_t i = 0; i < sizeof(opts); i++) if (opts[i] == step) return true;
    return false;
}

// Następny / poprzedni krok z listy TEMP_STEP_OPTIONS (dir = +1 / -1)
uint8_t config_temp_step_next(uint8_t step, int dir) {
    static const uint8_t opts[] = TEMP_STEP_OPTIONS;
    const int n = (int)sizeof(opts);
    int idx = 0;
    for (int i = 0; i < n; i++) if (opts[i] == step) idx = i;
    idx += dir;
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    return opts[idx];
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Pilnuje zakresów (np. po ręcznej edycji przez USB lub starej wersji)
void config_sanitize(Config_t *cfg) {
    if (cfg->temp_max < TEMP_MAX_MIN)  cfg->temp_max = TEMP_MAX_MIN;
    if (cfg->temp_max > TEMP_SET_MAX)  cfg->temp_max = TEMP_SET_MAX;
    if (cfg->power_limit < POWER_LIMIT_MIN) cfg->power_limit = POWER_LIMIT_MIN;
    if (cfg->power_limit > 100)        cfg->power_limit = 100;
    if (!config_temp_step_valid(cfg->temp_step)) cfg->temp_step = TEMP_STEP_DEFAULT;

    for (int p = 0; p < NUM_PROFILES; p++) {
        if (cfg->profile_temp[p] < TEMP_SET_MIN)  cfg->profile_temp[p] = TEMP_SET_MIN;
        if (cfg->profile_temp[p] > cfg->temp_max) cfg->profile_temp[p] = cfg->temp_max;
    }
    for (int i = 0; i < NUM_CHANNELS; i++) {
        ChannelConfig_t *cc = &cfg->ch[i];
        if (cc->setpoint < TEMP_SET_MIN) cc->setpoint = TEMP_SET_MIN;
        if (cc->setpoint > cfg->temp_max) cc->setpoint = cfg->temp_max;
        if (cc->active_profile < -1 || cc->active_profile >= NUM_PROFILES) cc->active_profile = -1;
        if (cc->temp_offset >  TEMP_OFFSET_LIMIT) cc->temp_offset =  TEMP_OFFSET_LIMIT;
        if (cc->temp_offset < -TEMP_OFFSET_LIMIT) cc->temp_offset = -TEMP_OFFSET_LIMIT;
        cc->kp = clampf(cc->kp, 0.0f, 200.0f);
        cc->ki = clampf(cc->ki, 0.0f, 20.0f);
        cc->kd = clampf(cc->kd, 0.0f, 100.0f);
    }
    uint32_t s = cfg->standby_delay_ms / 1000;
    if (s < STANDBY_DELAY_MIN_S) s = STANDBY_DELAY_MIN_S;
    if (s > STANDBY_DELAY_MAX_S) s = STANDBY_DELAY_MAX_S;
    cfg->standby_delay_ms = s * 1000;
    if (cfg->hibernate_temp < HIBERNATE_TEMP_MIN) cfg->hibernate_temp = HIBERNATE_TEMP_MIN;
    if (cfg->hibernate_temp > HIBERNATE_TEMP_MAX) cfg->hibernate_temp = HIBERNATE_TEMP_MAX;
    if (cfg->auto_off_min > AUTO_OFF_MAX_MIN) cfg->auto_off_min = AUTO_OFF_MAX_MIN;
    if (cfg->brightness < 10)  cfg->brightness = 10;
    if (cfg->brightness > 100) cfg->brightness = 100;
    if (cfg->selected_ch >= NUM_CHANNELS) cfg->selected_ch = 0;
}

// ============================================================
//  Odczyt konfiguracji z Flash
// ============================================================
void config_load(Config_t *cfg) {
    const Config_t *flash_cfg = (const Config_t *)(XIP_BASE + FLASH_CONFIG_OFFSET);
    Config_t tmp;
    memcpy(&tmp, flash_cfg, sizeof(Config_t));

    if (tmp.magic == FLASH_CONFIG_MAGIC && tmp.crc == config_crc(&tmp)) {
        memcpy(cfg, &tmp, sizeof(Config_t));
        config_sanitize(cfg);
    } else {
        // Pierwsze uruchomienie, zmiana wersji struktury lub uszkodzone dane
        config_set_defaults(cfg);
        config_save(cfg);
    }
}

// ============================================================
//  Zapis konfiguracji do Flash
//  Wymazanie sektora 4KB + zapis strony 256B, przerwania wyłączone.
//  Trwa ~50 ms - wywołuj tylko na żądanie (SAVE / autozapis).
// ============================================================
void config_save(Config_t *cfg) {
    cfg->magic = FLASH_CONFIG_MAGIC;
    cfg->crc   = config_crc(cfg);

    static uint8_t page_buf[FLASH_PAGE_SIZE];
    memset(page_buf, 0xFF, sizeof(page_buf));
    memcpy(page_buf, cfg, sizeof(Config_t));

    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(FLASH_CONFIG_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_CONFIG_OFFSET, page_buf, FLASH_PAGE_SIZE);
    restore_interrupts(irq);
}

// ============================================================
//  Pomiar napięcia wejściowego przez ADC
// ============================================================
void adc_voltage_init(void) {
    adc_init();
    adc_gpio_init(PIN_V_SENS);
}

float adc_read_voltage(void) {
    adc_select_input(ADC_V_SENS);
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) sum += adc_read();
    float v_adc = (sum / 16.0f / ADC_MAX) * V_REF;
    return v_adc * V_DIVIDER_RATIO;
}
