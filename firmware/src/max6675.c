#include "max6675.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

typedef struct {
    uint so_pin;
    uint cs_pin;
    uint sck_pin;
} Max6675Pins_t;

static const Max6675Pins_t TEMP_PINS[HW_CHANNELS] = {
    { .so_pin = PIN_TEMP1_SO, .cs_pin = PIN_TEMP1_CS, .sck_pin = PIN_TEMP1_SCK },
    { .so_pin = PIN_TEMP2_SO, .cs_pin = PIN_TEMP2_CS, .sck_pin = PIN_TEMP2_SCK },
};

_Static_assert(HW_CHANNELS == 2, "TEMP_PINS ma wpisy dla 2 kanalow");

static void max6675_init_channel(const Max6675Pins_t *pins) {
    gpio_init(pins->so_pin);
    gpio_set_dir(pins->so_pin, GPIO_IN);
    // Pull-down: odłączony moduł czyta się jako 0x0000 zamiast losowych danych
    gpio_pull_down(pins->so_pin);

    gpio_init(pins->cs_pin);
    gpio_set_dir(pins->cs_pin, GPIO_OUT);
    gpio_put(pins->cs_pin, 1);

    gpio_init(pins->sck_pin);
    gpio_set_dir(pins->sck_pin, GPIO_OUT);
    gpio_put(pins->sck_pin, 0);
}

void max6675_init(void) {
    for (int i = 0; i < HW_CHANNELS; i++) {     // CS nieużywanego modułu też w stanie wysokim
        max6675_init_channel(&TEMP_PINS[i]);
    }
}

void max6675_restart(int ch) {
    const Max6675Pins_t *pins = &TEMP_PINS[ch];
    gpio_put(pins->cs_pin, 0);
    sleep_us(2);
    gpio_put(pins->cs_pin, 1);
}

// Odczyt surowy 16-bit z MAX6675; -1 przy błędzie
static int16_t max6675_read_raw(const Max6675Pins_t *pins) {
    uint16_t raw = 0;

    gpio_put(pins->cs_pin, 0);
    sleep_us(2);  // tCSS min 100ns

    for (int i = 0; i < 16; i++) {
        gpio_put(pins->sck_pin, 1);
        sleep_us(1);
        raw <<= 1;
        if (gpio_get(pins->so_pin)) {
            raw |= 1u;
        }
        gpio_put(pins->sck_pin, 0);
        sleep_us(1);
    }

    gpio_put(pins->cs_pin, 1);  // start kolejnej konwersji

    // D15 zawsze 0, D1 (device ID) zawsze 0 - inaczej linia wisi w stanie HIGH
    if (raw & 0x8002) return -1;
    // D2 = termopara otwarta (brak kolby / uszkodzony grot)
    if (raw & 0x0004) return -1;
    // 0x0000 = brak modułu (SO ściągnięte do masy)
    if (raw == 0x0000) return -1;

    return (int16_t)((raw >> 3) & 0x0FFF);   // 0.25°C / LSB
}

float max6675_read(int ch, uint8_t *error_flag) {
    int16_t raw = max6675_read_raw(&TEMP_PINS[ch]);
    if (raw < 0) {
        *error_flag = 1;
        return 0.0f;
    }
    *error_flag = 0;
    return raw * 0.25f;
}
