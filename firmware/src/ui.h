#pragma once
#include "config.h"
#include "nextion.h"

// ============================================================
//  Strony HMI
// ============================================================
#define PAGE_MAIN       0
#define PAGE_SETTINGS   1
#define PAGE_SYSTEM     2       // kalibracja, moc, jasność, krok, max temp., licznik
#define PAGE_PID        3

// ============================================================
//  ID zdarzeń dotyku (printh 65 <page> <id> 01 w "Touch Press Event")
//  UWAGA: to NIE są ID komponentów Nextion - to numery wpisane ręcznie
//  w printh. Muszą się zgadzać z docs/DISPLAY.md
// ============================================================
// --- Strona 0 (main) ---
#define EV_MAIN_P1          1   // b_p1
#define EV_MAIN_P2          2   // b_p2
#define EV_MAIN_P3          3   // b_p3
#define EV_MAIN_MINUS       4   // b_minus  (nastawa -5)
#define EV_MAIN_PLUS        5   // b_plus   (nastawa +5)
#define EV_MAIN_SETTINGS    10  // b_cfg
#define EV_MAIN_SEL_CH1     20  // m_ch1    (hotspot - wybór kolby 1)
#define EV_MAIN_SEL_CH2     21  // m_ch2
#define EV_MAIN_PWR_CH1     22  // t_mode1  (wł./wył. kolby 1)
#define EV_MAIN_PWR_CH2     23  // t_mode2

// --- Strona 1 (ustawienia) --- nieparzyste = "+", parzyste = "-"
#define EV_SET_P1_UP        1
#define EV_SET_P1_DN        2
#define EV_SET_P2_UP        3
#define EV_SET_P2_DN        4
#define EV_SET_P3_UP        5
#define EV_SET_P3_DN        6
#define EV_SET_SLEEP_UP     7
#define EV_SET_SLEEP_DN     8
#define EV_SET_HIB_UP       9
#define EV_SET_HIB_DN       10
#define EV_SET_AOFF_UP      11
#define EV_SET_AOFF_DN      12
#define EV_SET_SYS_PAGE     13  // b_sys (-> strona 2)
#define EV_SET_SAVE         20
#define EV_SET_BACK         21

// --- Strona 2 (system) --- nieparzyste = "+", parzyste = "-"
#define EV_SYS_OFF_UP       1   // korekta temperatury
#define EV_SYS_OFF_DN       2
#define EV_SYS_PWR_UP       3   // limit mocy
#define EV_SYS_PWR_DN       4
#define EV_SYS_BRI_UP       5   // jasność
#define EV_SYS_BRI_DN       6
#define EV_SYS_STEP_UP      7   // krok nastawy
#define EV_SYS_STEP_DN      8
#define EV_SYS_TMAX_UP      9   // maks. temperatura
#define EV_SYS_TMAX_DN      10
#define EV_SYS_PID_PAGE     11  // b_pid (-> strona 3)
#define EV_SYS_SAVE         20
#define EV_SYS_BACK         21  // -> strona 1

// --- Strona 3 (PID) ---
#define EV_PID_KP_UP        1
#define EV_PID_KP_DN        2
#define EV_PID_KI_UP        3
#define EV_PID_KI_DN        4
#define EV_PID_KD_UP        5
#define EV_PID_KD_DN        6
#define EV_PID_CH_SWITCH    12
#define EV_PID_DEFAULTS     13
#define EV_PID_SAVE         20
#define EV_PID_BACK         21  // -> strona 2

// ============================================================
//  Paleta (RGB565) - ta sama co w tools/gen_nextion_ui.py
// ============================================================
#define RGB565(r, g, b)  ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#define CLR_BG          RGB565(0x0B, 0x0F, 0x14)
#define CLR_PANEL       RGB565(0x16, 0x1C, 0x24)
#define CLR_PANEL_HDR   RGB565(0x23, 0x2B, 0x36)
#define CLR_BTN         RGB565(0x2A, 0x33, 0x3F)
#define CLR_ACCENT      RGB565(0xFF, 0x7A, 0x1A)
#define CLR_TEXT        RGB565(0xE8, 0xED, 0xF2)
#define CLR_TEXT_DIM    RGB565(0x8A, 0x96, 0xA3)
#define CLR_GREEN       RGB565(0x2E, 0xCC, 0x71)
#define CLR_ORANGE      RGB565(0xFF, 0x9F, 0x1A)
#define CLR_BLUE        RGB565(0x3D, 0x8B, 0xFF)
#define CLR_YELLOW      RGB565(0xF5, 0xC5, 0x18)
#define CLR_RED         RGB565(0xFF, 0x3B, 0x30)
#define CLR_GRAY        RGB565(0x56, 0x60, 0x6B)
#define CLR_BLACK       0x0000

void ui_init(SystemState_t *sys);
// Obsługa odbioru + odświeżanie ekranu; wywoływać w każdym obiegu pętli
void ui_task(SystemState_t *sys, uint32_t now_ms);
// Krótki komunikat w t_status (strony 1 i 2)
void ui_status(const char *msg, uint32_t now_ms);
// Oznacz konfigurację do autozapisu
void ui_mark_dirty(SystemState_t *sys, uint32_t now_ms);
// Wybór kanału (z wyświetlacza lub długie przytrzymanie przycisku)
void ui_select_channel(SystemState_t *sys, int ch, uint32_t now_ms);
// Zapis do flash (natychmiastowy)
void ui_save_now(SystemState_t *sys, uint32_t now_ms);
