#include "buttons.h"
#include "hardware/gpio.h"

static const uint BTN_PINS[NUM_BUTTONS]   = {PIN_BUTTON_1, PIN_BUTTON_2, PIN_BUTTON_3};
static const uint LIGHT_PINS[NUM_BUTTONS] = {PIN_LIGHT_1,  PIN_LIGHT_2,  PIN_LIGHT_3};

typedef struct {
    uint8_t  stable;            // 1 = wciśnięty (po debounce)
    uint8_t  last_raw;
    uint32_t raw_since_ms;
    uint32_t pressed_at_ms;
    uint8_t  long_sent;
} Button_t;

static Button_t btn_state[NUM_BUTTONS];

void buttons_init(void) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        gpio_init(BTN_PINS[i]);
        gpio_set_dir(BTN_PINS[i], GPIO_IN);
        gpio_pull_up(BTN_PINS[i]);

        gpio_init(LIGHT_PINS[i]);
        gpio_set_dir(LIGHT_PINS[i], GPIO_OUT);
        button_light_set(i, false);
    }
}

void button_light_set(int btn, bool on) {
    gpio_put(LIGHT_PINS[btn], (on == (bool)LIGHT_ACTIVE_HIGH) ? 1 : 0);
}

ButtonEvent_t buttons_poll(int btn, uint32_t now_ms) {
    Button_t *b = &btn_state[btn];
    uint8_t raw = gpio_get(BTN_PINS[btn]) ? 0 : 1;     // active low

    if (raw != b->last_raw) {
        b->last_raw     = raw;
        b->raw_since_ms = now_ms;
    }

    if (raw != b->stable && (now_ms - b->raw_since_ms) >= BUTTON_DEBOUNCE_MS) {
        b->stable = raw;
        if (raw) {
            b->pressed_at_ms = now_ms;
            b->long_sent     = 0;
        } else if (!b->long_sent) {
            return BTN_EV_SHORT;
        }
    }

    if (b->stable && !b->long_sent && (now_ms - b->pressed_at_ms) >= BUTTON_LONG_MS) {
        b->long_sent = 1;
        return BTN_EV_LONG;
    }
    return BTN_EV_NONE;
}

// ============================================================
//  LED:  świeci aktywny profil wybranego kanału
//        wolne miganie - czuwanie / hibernacja
//        wszystkie szybko migają - błąd
//        zgaszone - kanał wyłączony lub nastawa ręczna
// ============================================================
void button_lights_update(const SystemState_t *sys, uint32_t now_ms) {
    const Channel_t *ch = &sys->ch[sys->selected_ch];
    bool slow = ((now_ms / 500) & 1) != 0;
    bool fast = ((now_ms / 150) & 1) != 0;

    bool fault = (ch->fault == FAULT_OVERTEMP || ch->fault == FAULT_RUNAWAY);

    for (int i = 0; i < NUM_BUTTONS; i++) {
        bool on;
        if (fault) {
            on = fast;
        } else if (ch->mode == MODE_OFF) {
            on = false;
        } else if (ch->active_profile == i) {
            on = (ch->mode == MODE_STANDBY || ch->mode == MODE_HIBERNATE) ? slow : true;
        } else {
            on = false;
        }
        button_light_set(i, on);
    }
}
