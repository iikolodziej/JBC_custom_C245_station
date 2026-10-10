#pragma once
#include "config.h"

#define NUM_BUTTONS         3
#define BUTTON_DEBOUNCE_MS  30
#define BUTTON_LONG_MS      1000

typedef enum {
    BTN_EV_NONE = 0,
    BTN_EV_SHORT,       // puszczony przed BUTTON_LONG_MS
    BTN_EV_LONG,        // trzymany BUTTON_LONG_MS (zgłaszane raz, w trakcie trzymania)
} ButtonEvent_t;

void buttons_init(void);
// Zwraca zdarzenie dla przycisku btn (0..2), wywoływać cyklicznie
ButtonEvent_t buttons_poll(int btn, uint32_t now_ms);

// LED w przyciskach
void button_light_set(int btn, bool on);
// Aktualizacja LED wg stanu wybranego kanału (miganie przy czuwaniu/błędzie)
void button_lights_update(const SystemState_t *sys, uint32_t now_ms);
