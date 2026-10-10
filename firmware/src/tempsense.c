#include "tempsense.h"
#include "max6675.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include <math.h>

#if TEMP_SENSOR == TEMP_SENSOR_MAX6675
// ============================================================
//  MAX6675 - bez zmian względem poprzednich wersji
// ============================================================
void  tsens_init(void)                     { max6675_init(); }
void  tsens_start(int ch)                  { max6675_restart(ch); }
float tsens_read(int ch, uint8_t *err)     { return max6675_read(ch, err); }
void  tsens_debug_print(void)              { printf("TC: MAX6675 (termopara K)\n"); }
void  tsens_i2c_scan(void)                 { printf("i2c: nieuzywane (MAX6675)\n"); }

#else
// ============================================================
//  ADS1115 + termopara C245
// ============================================================

// Rejestry ADS1115
#define ADS_REG_CONV    0x00
#define ADS_REG_CFG     0x01

// Bity rejestru konfiguracji
#define ADS_OS_START    0x8000          // start pojedynczej konwersji / 1 = gotowe
#define ADS_MUX_A0_A1   (0u << 12)      // różnicowo AIN0 - AIN1 (kolba 1)
#define ADS_MUX_A2_A3   (3u << 12)      // różnicowo AIN2 - AIN3 (kolba 2)
#define ADS_MUX_A2_GND  (6u << 12)      // AIN2 do masy (NTC zimnego złącza)
#define ADS_PGA_0256    (5u << 9)       // ±0.256 V -> 7.8125 uV / LSB
#define ADS_PGA_4096    (1u << 9)       // ±4.096 V -> 125 uV / LSB
#define ADS_MODE_SINGLE 0x0100
#define ADS_DR_128      (4u << 5)       // 128 SPS - ok. 7.8 ms (termopara, mniej szumu)
#define ADS_DR_860      (7u << 5)       // 860 SPS - ok. 1.2 ms (NTC)
#define ADS_COMP_OFF    0x0003

#define ADS_TC_UV_PER_LSB   7.8125f
#define ADS_I2C_TIMEOUT_US  3000
#define ADS_RAW_OPEN        32000       // blisko pełnej skali = otwarta termopara (podciągnięte A0)

// Termopara C245: krzywa z projektu AxxSolder (wzmocnienie 240, ADC 12 bit / 3.3 V)
// przeliczona na napięcie termopary: dT[°C] = K1 * U[uV] + K2 * U[uV]^2
// (ok. 28 uV/°C, ok. 9.2 mV przy 350 °C z zimnym złączem ~24 °C)
#define C245_K1     0.035747f
#define C245_K2    -4.1975e-8f

static const uint16_t TC_MUX[2] = { ADS_MUX_A0_A1, ADS_MUX_A2_A3 };

static uint8_t  g_addr     = 0;     // 0 = nie znaleziono
static bool     g_pending[2];
static int16_t  g_last_raw[2];
static float    g_last_uv[2];
static float    g_last_temp[2];
static uint8_t  g_last_err[2];
static float    g_cj_c     = 25.0f; // temperatura zimnego złącza
static bool     g_cj_init  = false;
static uint32_t g_i2c_errors = 0;

_Static_assert(!(CJC_SOURCE == CJC_NTC_A2 && NUM_CHANNELS > 1),
               "NTC na A2 koliduje z termoparą kolby 2 (A2-A3) - użyj CJC_PICO_INTERNAL");

static bool ads_write_reg(uint8_t reg, uint16_t val) {
    uint8_t buf[3] = { reg, (uint8_t)(val >> 8), (uint8_t)val };
    int n = i2c_write_timeout_us(ADS_I2C, g_addr, buf, 3, false, ADS_I2C_TIMEOUT_US);
    if (n != 3) { g_i2c_errors++; return false; }
    return true;
}

static bool ads_read_reg(uint8_t reg, uint16_t *val) {
    uint8_t buf[2];
    if (i2c_write_timeout_us(ADS_I2C, g_addr, &reg, 1, true, ADS_I2C_TIMEOUT_US) != 1 ||
        i2c_read_timeout_us(ADS_I2C, g_addr, buf, 2, false, ADS_I2C_TIMEOUT_US) != 2) {
        g_i2c_errors++;
        return false;
    }
    *val = (uint16_t)((buf[0] << 8) | buf[1]);
    return true;
}

// Czeka na koniec konwersji (bit OS = 1), maks. timeout_ms
static bool ads_wait_ready(uint32_t timeout_ms) {
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    uint16_t cfg;
    do {
        if (!ads_read_reg(ADS_REG_CFG, &cfg)) return false;
        if (cfg & ADS_OS_START) return true;
        sleep_us(200);
    } while (absolute_time_diff_us(get_absolute_time(), deadline) > 0);
    return false;
}

static bool ads_single(uint16_t mux_pga_dr, int16_t *raw) {
    if (!ads_write_reg(ADS_REG_CFG, ADS_OS_START | mux_pga_dr | ADS_MODE_SINGLE | ADS_COMP_OFF)) return false;
    if (!ads_wait_ready(15)) return false;
    uint16_t v;
    if (!ads_read_reg(ADS_REG_CONV, &v)) return false;
    *raw = (int16_t)v;
    return true;
}

// Odblokowanie magistrali i ponowna inicjalizacja I2C: 9 taktów SCL (moduł mógł
// zostać w połowie bajtu i trzymać SDA), warunek STOP, potem i2c_init od nowa.
// Potrzebne po teście linii (piny chwilowo jako GPIO) i po zakłóceniu w czasie pracy.
static void i2c_bus_reset(void) {
    i2c_deinit(ADS_I2C);
    gpio_set_function(PIN_ADS_SDA, GPIO_FUNC_SIO);
    gpio_set_function(PIN_ADS_SCL, GPIO_FUNC_SIO);
    gpio_pull_up(PIN_ADS_SDA);
    gpio_pull_up(PIN_ADS_SCL);
    gpio_put(PIN_ADS_SDA, 0);
    gpio_put(PIN_ADS_SCL, 0);
    gpio_set_dir(PIN_ADS_SDA, GPIO_IN);         // SDA puszczona (open-drain: IN = 1, OUT = 0)
    for (int i = 0; i < 9; i++) {
        gpio_set_dir(PIN_ADS_SCL, GPIO_OUT); sleep_us(5);
        gpio_set_dir(PIN_ADS_SCL, GPIO_IN);  sleep_us(5);
    }
    gpio_set_dir(PIN_ADS_SDA, GPIO_OUT); sleep_us(5);   // STOP: SDA 0 -> 1 przy SCL = 1
    gpio_set_dir(PIN_ADS_SDA, GPIO_IN);  sleep_us(5);
    i2c_init(ADS_I2C, ADS_I2C_BAUD);
    gpio_set_function(PIN_ADS_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_ADS_SCL, GPIO_FUNC_I2C);
}

// Adres zależy od pinu ADDR (GND 0x48, VDD 0x49, SDA 0x4A, SCL 0x4B).
// Moduły zwykle mają rezystor ADDR->GND, ale szukamy wszystkich czterech.
static void ads_probe(void) {
    static bool warned = false;
    i2c_bus_reset();
    for (uint8_t a = 0x48; a <= 0x4B; a++) {
        uint8_t reg = ADS_REG_CFG, buf[2];
        if (i2c_write_timeout_us(ADS_I2C, a, &reg, 1, true, ADS_I2C_TIMEOUT_US) == 1 &&
            i2c_read_timeout_us(ADS_I2C, a, buf, 2, false, ADS_I2C_TIMEOUT_US) == 2) {
            g_addr = a;
            printf("[TC] ADS1115 znaleziony pod adresem 0x%02X\n", a);
            warned = false;
            return;
        }
    }
    g_addr = 0;
    if (!warned) {                          // komunikat raz, nie co pomiar
        warned = true;
        printf("[TC] !!! ADS1115 nie odpowiada (sprawdz SDA=GP%d, SCL=GP%d, zasilanie 3.3V)\n",
               PIN_ADS_SDA, PIN_ADS_SCL);
    }
}

// ------------------------------------------------------------
//  Zimne złącze
// ------------------------------------------------------------
static float cj_read_pico(void) {
#ifdef ADC_TEMPERATURE_CHANNEL_NUM
    adc_select_input(ADC_TEMPERATURE_CHANNEL_NUM);  // wbudowany czujnik temperatury (RP2040/RP2350)
#else
    adc_select_input(4);
#endif
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) sum += adc_read();
    float v = (sum / 16.0f) * 3.3f / 4095.0f;
    return 27.0f - (v - 0.706f) / 0.001721f;
}

#if CJC_SOURCE == CJC_NTC_A2
static bool cj_read_ntc(float *t) {
    int16_t raw;
    if (!ads_single(ADS_MUX_A2_GND | ADS_PGA_4096 | ADS_DR_860, &raw)) return false;
    float v = raw * 4.096f / 32768.0f;
    if (v <= 0.01f || v >= NTC_VDD - 0.01f) return false;       // NTC odłączony / zwarty
    float r = NTC_R_SERIES * v / (NTC_VDD - v);                  // NTC od A2 do GND
    float inv_t = 1.0f / 298.15f + logf(r / NTC_R25) / NTC_BETA;
    *t = 1.0f / inv_t - 273.15f;
    return true;
}
#endif

static void cj_update(void) {
    float t;
#if CJC_SOURCE == CJC_NTC_A2
    if (!cj_read_ntc(&t)) t = cj_read_pico();
#else
    t = cj_read_pico();
#endif
    if (!g_cj_init) { g_cj_c = t; g_cj_init = true; }
    else            g_cj_c = 0.9f * g_cj_c + 0.1f * t;      // wolny filtr - złącze zmienia się powoli
}

// ------------------------------------------------------------
//  API
// ------------------------------------------------------------
void tsens_init(void) {
    i2c_init(ADS_I2C, ADS_I2C_BAUD);
    gpio_set_function(PIN_ADS_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_ADS_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_ADS_SDA);              // moduły mają własne podciągnięcie - to tylko zapas
    gpio_pull_up(PIN_ADS_SCL);

    adc_set_temp_sensor_enabled(true);      // adc_init() wywołane wcześniej (adc_voltage_init)

    ads_probe();
    cj_update();
}

void tsens_start(int ch) {
    g_pending[ch] = false;
    if (!g_addr) return;
    // Przy dwóch kolbach jeden ADS mierzy po kolei - startujemy tylko pierwszą,
    // kolejne mierzą się w tsens_read() (ok. 8 ms każda).
    if (ch != 0) return;
    g_pending[ch] = ads_write_reg(ADS_REG_CFG, ADS_OS_START | TC_MUX[ch] | ADS_PGA_0256 |
                                               ADS_DR_128 | ADS_MODE_SINGLE | ADS_COMP_OFF);
}

float tsens_read(int ch, uint8_t *err) {
    *err = 1;
    if (!g_addr) {
        ads_probe();                        // może moduł doszedł / wrócił
        g_last_err[ch] = 1;
        return 0.0f;
    }

    int16_t raw = 0;
    bool ok;
    if (g_pending[ch]) {
        uint16_t v = 0;
        ok = ads_wait_ready(15) && ads_read_reg(ADS_REG_CONV, &v);
        raw = (int16_t)v;
    } else {
        ok = ads_single(TC_MUX[ch] | ADS_PGA_0256 | ADS_DR_128, &raw);
    }
    g_pending[ch] = false;

    if (ch == 0) cj_update();               // raz na cykl pomiarowy

    if (!ok) { g_last_err[ch] = 1; return 0.0f; }

#if ADS_TC_INVERT
    raw = (int16_t)(-raw);
#endif
    g_last_raw[ch] = raw;

    // Otwarta termopara: zielony przewód podciągnięty (1 MOhm do 3.3 V, przed rezystorem 2,2k) -> pełna skala
    if (raw >= ADS_RAW_OPEN || raw <= -ADS_RAW_OPEN) { g_last_err[ch] = 1; return 0.0f; }

    float uv = raw * ADS_TC_UV_PER_LSB;
    float t  = g_cj_c + C245_K1 * uv + C245_K2 * uv * uv;
    g_last_uv[ch]   = uv;
    g_last_temp[ch] = t;

    if (t < -20.0f || t > 700.0f) { g_last_err[ch] = 1; return 0.0f; }   // bzdura = brak grota

    g_last_err[ch] = 0;
    *err = 0;
    return t;
}

// Stan linii: z podciąganiem Pico w dół i w górę.
//  1/1 = linia trzymana wysoko (rezystor podciągający modułu - poprawnie)
//  0/1 = nic nie jest podłączone (brak modułu / przerwa w przewodzie)
//  0/0 = linia zwarta do masy (uszkodzony moduł albo pin Pico)
static void line_test(uint pin, const char *name) {
    // "lo": linia rozładowana do 0, potem puszczona bez rezystorów Pico - po 20 us wysoko
    // tylko jeśli trzyma ją rezystor modułu. (Zamiast pull-down: w RP2350 pull-down
    // nie działa poprawnie - erratum E9.)
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_disable_pulls(pin);
    gpio_put(pin, 0);
    gpio_set_dir(pin, GPIO_OUT); sleep_ms(1);
    gpio_set_dir(pin, GPIO_IN);  sleep_us(20);
    int lo = gpio_get(pin);
    gpio_pull_up(pin);   sleep_ms(2);
    int hi = gpio_get(pin);
    const char *v = (lo && hi) ? "OK - podciagnieta przez modul"
                  : (!lo && hi) ? "PRZERWA - nic nie trzyma linii (brak polaczenia z modulem?)"
                  : (!lo && !hi) ? "ZWARCIE DO MASY"
                  : "dziwny stan";
    printf("  %s (GP%u): sam modul/z pull-up = %d/%d  -> %s\n", name, pin, lo, hi, v);
}

void tsens_i2c_scan(void) {
    printf("I2C: test linii\n");
    line_test(PIN_ADS_SDA, "SDA");
    line_test(PIN_ADS_SCL, "SCL");
    i2c_bus_reset();
    printf("I2C: skan adresow 0x08-0x77:");
    int found = 0;
    for (uint8_t a = 0x08; a < 0x78; a++) {
        uint8_t b;
        if (i2c_read_timeout_us(ADS_I2C, a, &b, 1, false, 2000) >= 0) { printf(" 0x%02X", a); found++; }
    }
    printf(found ? "\n" : " brak urzadzen\n");
    ads_probe();
}

void tsens_debug_print(void) {
    if (!g_addr) {
        printf("TC: ADS1115 NIE ZNALEZIONY (bledy I2C=%lu)\n", (unsigned long)g_i2c_errors);
        return;
    }
    printf("TC: ADS1115 @0x%02X  zimne zlacze=%.1f C (%s)  bledy I2C=%lu\n", g_addr, g_cj_c,
           CJC_SOURCE == CJC_NTC_A2 ? "NTC A2" : "czujnik Pico", (unsigned long)g_i2c_errors);
    for (int i = 0; i < NUM_CHANNELS; i++) {
        printf("  kolba %d: raw=%d  U=%.1f uV  T=%.1f C  %s\n", i + 1, g_last_raw[i], g_last_uv[i],
               g_last_temp[i], g_last_err[i] ? "BLAD / BRAK GROTA" : "OK");
    }
}
#endif
