#pragma once
#include "config.h"

// Prędkość robocza. Firmware sam wykrywa aktualną prędkość wyświetlacza
// (9600 domyślnie w nowym HMI) i przełącza go na tę wartość komendą baud=.
#define NEXTION_BAUD_RUN    115200

typedef enum {
    NX_EV_NONE = 0,
    NX_EV_TOUCH,        // 0x65 page comp event  (printh 65 PP CC 01)
    NX_EV_PAGE,         // 0x66 page             (odpowiedź na sendme)
    NX_EV_RESET,        // 0x00 00 00 / 0x88     (wyświetlacz się zrestartował)
    NX_EV_XY,           // 0x67 xH xL yH yL ev   (dotyk przy sendxy=1)
} NxEventType_t;

typedef struct {
    NxEventType_t type;
    uint8_t page;
    uint8_t comp;
    uint8_t press;      // 1 = press, 0 = release
    uint16_t x, y;      // NX_EV_XY
} NextionEvent_t;

// Blokuje do ~2 s (wykrywanie prędkości). Grzałki muszą być wyłączone!
void nextion_init(void);
bool nextion_link_ok(void);
// Statystyki odbioru z wyświetlacza (konsola: "nx")
void nextion_debug_print(void);

void nextion_send_cmd(const char *cmd);
void nextion_cmdf(const char *fmt, ...);

// Wysyłają tylko jeśli wartość się zmieniła od ostatniego wysłania (cache)
void nextion_set_txt(const char *obj, const char *txt);
void nextion_set_val(const char *obj, int32_t val);
void nextion_set_attr(const char *obj, const char *attr, int32_t val);
void nextion_cache_clear(void);
// true = wartość pod kluczem się zmieniła (i zapamiętuje nową)
bool nextion_cache_changed(const char *key, uint32_t hash);
uint32_t nextion_hash(const char *s, uint32_t seed);

void nextion_goto_page(uint8_t page);

// Nieblokujące; zwraca true gdy odebrano kompletną ramkę
bool nextion_poll_event(NextionEvent_t *ev);
