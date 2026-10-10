#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"

#define FW_VERSION          "2.3.1"

// ============================================================
//  MAPA PINÓW  (wg fizycznych pinów Raspberry Pi Pico)
//  Źródło: "jbc stacja pi pico pinout.docx"
// ============================================================
#define PIN_NONE            0xFF

// --- UART: Nextion (fizyczne piny 1-2) ---
#define UART_NEXTION        uart0
#define PIN_NEXTION_TX      0   // pin 1  / GP0  -> RXD wyswietlacza Nextion
#define PIN_NEXTION_RX      1   // pin 2  / GP1  <- TXD wyswietlacza Nextion

// --- LED w przyciskach profili ---
// Sprawdzone na sprzęcie 2026-09-24: diody przesunięte względem pinout.docx
#define PIN_LIGHT_1         21  // pin 27 / GP21 - dioda przy przycisku 1
#define PIN_LIGHT_2         22  // pin 29 / GP22 - dioda przy przycisku 2
#define PIN_LIGHT_3         2   // pin 4  / GP2  - dioda przy przycisku 3
#define LIGHT_ACTIVE_HIGH   1   // 1: HIGH = LED świeci (wejście ULN2603), 0: odwrotnie

// --- Czujniki odlozenia kolby (zwarcie do GND = odlozona) ---
// UWAGA: pinout.docx mówi: pin 5 (GP3) = kolba 1, pin 32 (GP27) = kolba 2.
// Poprzednia wersja firmware (testowana) używała GP27 dla kolby 1 - zostawione.
// Jeśli stojaki reagują "na krzyż" - zamień te dwie wartości.
#define PIN_STAT_IN1        27  // pin 32 / GP27
#define PIN_STAT_IN2        3   // pin 5  / GP3
#define STAT_IN_ACTIVE_LOW  1   // 1: aktywne po zwarciu do GND, 0: aktywne stanem HIGH

// --- Detekcja styku przy wymianie grota (tylko kolba 1) ---
#define PIN_TIP_CHANGE1     28  // pin 34 / GP28
#define PIN_TIP_CHANGE2     PIN_NONE
#define TIP_CHANGE_ACTIVE_LOW 1

// --- RTC / HW-111 (nieuzywane) ---
#define PIN_RTC_SDA         4   // pin 6  / GP4
#define PIN_RTC_SCL         5   // pin 7  / GP5

// ============================================================
//  CZUJNIK TEMPERATURY GROTA
//    TEMP_SENSOR_ADS1115 - ADS1115 na I2C1 (GP6 = SDA, GP7 = SCL), termopara różnicowo A0-A1
//    TEMP_SENSOR_MAX6675 - moduł MAX6675 na GP6/7/8 (jak w v2.1)
// ============================================================
#define TEMP_SENSOR_MAX6675 1
#define TEMP_SENSOR_ADS1115 2
#define TEMP_SENSOR         TEMP_SENSOR_ADS1115

// 2026-10-10: nowe Pico 2 - ADS1115 z powrotem na i2c1 GP6/GP7.
// (na starym Pico GP6 był uszkodzony, wtedy używane i2c0 GP8/GP9)
#define ADS_I2C             i2c1
#define PIN_ADS_SDA         6   // pin 9  / GP6
#define PIN_ADS_SCL         7   // pin 10 / GP7
#define ADS_I2C_BAUD        400000
#define ADS_TC_INVERT       1   // 1 = odwrotna biegunowość termopary (sprawdzone 2026-09-29: zielony na A0 daje minus)

// Zimne złącze: wbudowany czujnik Pico albo termistor NTC 10k na A2
// (3.3 V -[NTC_R_SERIES]- A2 -[NTC]- GND). Przy braku NTC wraca do czujnika Pico.
#define CJC_PICO_INTERNAL   0
#define CJC_NTC_A2          1
#define CJC_SOURCE          CJC_PICO_INTERNAL
#define NTC_R_SERIES        10000.0f
#define NTC_R25             10000.0f
#define NTC_BETA            3950.0f
#define NTC_VDD             3.3f

// --- MAX6675 kanał 1 (osobne linie, bit-bang) ---
#define PIN_TEMP1_SO        6   // pin 9  / GP6
#define PIN_TEMP1_CS        7   // pin 10 / GP7
#define PIN_TEMP1_SCK       8   // pin 11 / GP8

// --- Karta SD (nieuzywana) ---
#define PIN_SD_CS           9   // pin 12 / GP9
#define PIN_SD_SCK          10  // pin 14 / GP10
#define PIN_SD_MOSI         11  // pin 15 / GP11
#define PIN_SD_MISO         12  // pin 16 / GP12

// --- MAX6675 kanał 2 (osobne linie, bit-bang) ---
#define PIN_TEMP2_SO        13  // pin 17 / GP13
#define PIN_TEMP2_CS        14  // pin 19 / GP14
#define PIN_TEMP2_SCK       15  // pin 20 / GP15

// --- PWM: Sterowanie MOSFET grzałkami ---
#define PIN_PWM_CH2         16  // pin 21 / GP16 -> MOSFET kolby 2
#define PIN_PWM_CH1         17  // pin 22 / GP17 -> MOSFET kolby 1

// --- Przyciski fizyczne wyboru profilu ---
// Sprawdzone na sprzęcie 2026-09-24: zgodne z pinout.docx
#define PIN_BUTTON_1        18  // pin 24 / GP18 - fizyczny przycisk 1
#define PIN_BUTTON_2        19  // pin 25 / GP19 - fizyczny przycisk 2
#define PIN_BUTTON_3        20  // pin 26 / GP20 - fizyczny przycisk 3

// --- ADC: Pomiar napięcia wejściowego ---
#define PIN_V_SENS          26  // pin 31 / GP26 (ADC0)
#define ADC_V_SENS          0
#define V_DIVIDER_RATIO     8.02f  // R1=33k, R2=4.7k -> (33+4.7)/4.7
#define V_REF               3.3f
#define ADC_MAX             4095.0f
#define VIN_LOW_WARN        18.0f  // poniżej - ostrzeżenie na ekranie

#define PIN_CONFLICT(a, b)  ((a) == (b))
#if PIN_CONFLICT(PIN_STAT_IN1, PIN_STAT_IN2) || PIN_CONFLICT(PIN_PWM_CH1, PIN_PWM_CH2) || \
    PIN_CONFLICT(PIN_STAT_IN1, PIN_TIP_CHANGE1) || PIN_CONFLICT(PIN_STAT_IN2, PIN_TIP_CHANGE1) || \
    PIN_CONFLICT(PIN_NEXTION_RX, PIN_STAT_IN2) || PIN_CONFLICT(PIN_LIGHT_1, PIN_STAT_IN2)
#error "Konflikt pinow - sprawdz mape w config.h"
#endif

// ============================================================
//  KANAŁY / PROFILE
// ============================================================
// HW_CHANNELS  - ile kolb ma płytka (piny obu są zdefiniowane wyżej)
// NUM_CHANNELS - ile kolb obsługuje program i wyświetlacz.
//   1 = wersja na jedną kolbę (kolba 1). Wyjście PWM kolby 2 jest
//       na stałe trzymane w stanie niskim.
//   2 = obie kolby - wymaga HMI w wersji "2 kolby" (generator: define_main_2 w firmware/tools/gen_nextion_ui.py)
#define HW_CHANNELS         2
#ifndef NUM_CHANNELS
#define NUM_CHANNELS        1
#endif
#if NUM_CHANNELS < 1 || NUM_CHANNELS > HW_CHANNELS
#error "NUM_CHANNELS musi byc 1..HW_CHANNELS"
#endif
#define NUM_PROFILES        3

// ============================================================
//  ZAKRESY I BEZPIECZEŃSTWO
// ============================================================
#define TEMP_SET_MIN        150
#define TEMP_SET_MAX        480     // twardy limit; użytkownik ustawia niższy (temp_max)
#define TEMP_STEP           5       // krok temp. hibernacji
#define TEMP_MAX_MIN        200     // zakres ustawienia "max temperatura grota"
#define TEMP_MAX_STEP       10
#define POWER_LIMIT_MIN     10      // limit mocy grzania [%]
#define POWER_LIMIT_STEP    5
#define BRIGHTNESS_STEP     10
#define WORK_SAVE_PERIOD_S  600     // licznik godzin: zapis do flash co 10 min grzania
#define TEMP_OVERHEAT_C     520     // twarde wyłączenie grzania (latch)
#define TEMP_OFFSET_LIMIT   50      // kalibracja +/- [°C]

#define RUNAWAY_DUTY_PCT    80      // PID >= 80% dozwolonej mocy ...
#define RUNAWAY_TIME_MS     15000   // ... przez 15 s ...
#define RUNAWAY_MIN_RISE_C  15      // ... a temp. wzrosła < 15°C => BŁĄD GRZANIA

// ============================================================
//  PĘTLA POMIARU / GRZANIA
//
//  Grot JBC C245 ma termoparę i grzałkę na wspólnym przewodzie.
//  Pomiar przy płynącym prądzie grzałki jest przekłamany, więc
//  (TC_SYNC_MEASURE=1) cykl wygląda tak:
//
//   |<--- HEAT_WINDOW_MS --->|<SETTLE>|<---- MAX6675_CONV_MS ---->|
//   |  PWM = wyjście PID     | PWM=0  | PWM=0, MAX6675 mierzy     | -> odczyt, PID
//
//  MAX6675 potrzebuje ~220 ms na konwersję - czytany częściej
//  zwraca ciągle ten sam (stary) wynik.
//  Max. moc średnia = HEAT / (HEAT+SETTLE+CONV) ≈ 60%.
//
//  TC_SYNC_MEASURE=0: PWM ciągłe, pomiar co TC_READ_PERIOD_MS
//  (tylko jeśli termopara jest galwanicznie oddzielona od grzałki).
// ============================================================
#define TC_SYNC_MEASURE     1
#define HEAT_WINDOW_MS      350
#define MAX6675_CONV_MS     230
#define TC_READ_PERIOD_MS   250

// ADS1115 mierzy w ok. 8 ms, więc przerwa na pomiar jest krótka (max. moc ok. 94%)
#if TEMP_SENSOR == TEMP_SENSOR_ADS1115
#define TC_SETTLE_MS        10      // wygaszenie zakłóceń po wyłączeniu grzałki
#define TSENS_CONV_MS       10
#else
#define TC_SETTLE_MS        5
#define TSENS_CONV_MS       MAX6675_CONV_MS
#endif

#define PWM_FREQ_HZ         1000
#define PWM_MAX_DUTY        1000    // 0-1000 = 0-100%

// ============================================================
//  CZUWANIE
// ============================================================
#define STANDBY_DEBOUNCE_MS         500
#define TIP_CHANGE_BLOCK_MS         5000
#define HIBERNATE_TEMP_MIN          50
#define HIBERNATE_TEMP_MAX          250
#define STANDBY_DELAY_MIN_S         5
#define STANDBY_DELAY_MAX_S         300
#define STANDBY_DELAY_STEP_S        5
#define AUTO_OFF_MAX_MIN            120     // 0 = nigdy
#define AUTO_OFF_STEP_MIN           5

// ============================================================
//  REGULATOR PID
// ============================================================
typedef struct {
    float kp;
    float ki;
    float kd;
    float integral;         // w jednostkach wyjścia (0-1000)
    float prev_meas;
    uint8_t has_prev;
    float integral_limit;
    float output_limit;
} PID_t;

#define PID_DEFAULT_KP      15.0f
#define PID_DEFAULT_KI      2.5f
#define PID_DEFAULT_KD      14.0f

// ============================================================
//  FLASH - zapis konfiguracji (ostatni sektor pamięci)
// ============================================================
// Zmiana struktury lub liczby kanałów => nowy magic => ustawienia domyślne
#define FLASH_CONFIG_MAGIC      (0xBC245110u + NUM_CHANNELS)   // v2.1: strona "system"

typedef struct {
    uint16_t setpoint;          // ostatnia nastawa
    int8_t   active_profile;    // 0-2, -1 = nastawa ręczna
    int8_t   temp_offset;       // kalibracja [°C]
    float    kp, ki, kd;
    uint32_t work_s;            // licznik czasu grzania [s]
} ChannelConfig_t;

typedef struct {
    uint32_t        magic;
    uint16_t        profile_temp[NUM_PROFILES];
    uint16_t        hibernate_temp;
    ChannelConfig_t ch[NUM_CHANNELS];
    uint32_t        standby_delay_ms;   // odłożenie -> hibernacja
    uint16_t        auto_off_min;       // hibernacja -> wyłączenie (0 = nigdy)
    uint8_t         brightness;         // 10-100
    uint8_t         selected_ch;
    uint8_t         power_limit;        // maks. moc grzania 10-100 [%]
    uint8_t         temp_step;          // krok zmiany nastawy [°C] (TEMP_STEP_OPTIONS)
    uint16_t        temp_max;           // maks. nastawa [°C], TEMP_MAX_MIN..TEMP_SET_MAX
    uint32_t        crc;                // MUSI być ostatnie
} Config_t;

// ============================================================
//  STAN KANAŁU (runtime)
// ============================================================
typedef enum {
    MODE_ACTIVE,        // grzanie do nastawy
    MODE_STANDBY,       // kolba odłożona, trzyma nastawę do upływu czasu
    MODE_HIBERNATE,     // obniżona temperatura
    MODE_EXCHANGE,      // wymiana grota - grzanie zablokowane
    MODE_OFF            // wyłączony (ręcznie lub auto-off)
} ChannelMode_t;

typedef enum {
    FAULT_NONE = 0,
    FAULT_NO_TIP,       // termopara otwarta / brak kolby (samo się kasuje)
    FAULT_OVERTEMP,     // przegrzanie (latch)
    FAULT_RUNAWAY,      // grzeje a temp. nie rośnie (latch)
} Fault_t;

typedef struct {
    uint8_t  idx;

    float    temp_current;          // przefiltrowana + offset [°C]
    uint8_t  filter_init;
    uint16_t setpoint;              // nastawa użytkownika
    int8_t   active_profile;        // -1 = ręczna

    uint16_t pwm_duty;              // wyjście PID 0-1000
    float    power_percent;         // rzeczywista moc średnia 0-100

    ChannelMode_t mode;
    Fault_t  fault;
    uint8_t  off_by_user;           // 1 = wyłączony ręcznie (podniesienie nie budzi)

    // Czujnik odłożenia
    uint8_t  iron_docked;
    uint8_t  dock_raw_state;
    uint32_t dock_raw_since_ms;
    uint32_t docked_since_ms;
    uint32_t hibernate_since_ms;

    // Wymiana grota
    uint8_t  tip_change_active;
    uint8_t  tip_change_prev;
    uint32_t tip_change_block_until_ms;

    // Ochrona przed "ucieczką" (grzeje a nie rośnie)
    uint8_t  sat_tracking;
    uint32_t sat_since_ms;
    float    sat_start_temp;

    PID_t    pid;
} Channel_t;

// ============================================================
//  GLOBALNY STAN SYSTEMU
// ============================================================
typedef struct {
    Channel_t   ch[NUM_CHANNELS];
    Config_t    config;
    float       v_input;
    uint8_t     selected_ch;
    uint8_t     config_dirty;       // zmiany do autozapisu (profil/nastawa)
    uint32_t    dirty_since_ms;
    uint8_t     log_enabled;        // CSV przez USB (strojenie PID)
} SystemState_t;

extern SystemState_t g_sys;

// Dozwolone kroki zmiany nastawy [°C]
#define TEMP_STEP_OPTIONS   {1, 2, 5, 10, 20, 50}
#define TEMP_STEP_DEFAULT   5

static inline uint16_t channel_target_temp(const Channel_t *ch, const Config_t *cfg) {
    if (ch->mode == MODE_HIBERNATE && cfg->hibernate_temp < ch->setpoint)
        return cfg->hibernate_temp;
    return ch->setpoint;
}
