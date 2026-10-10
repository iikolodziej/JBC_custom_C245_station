#include "ui.h"
#include "channel.h"
#include "config_io.h"
#include "pid.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ============================================================
//  Wygląd ekranu jest w projekcie Nextion (HMI). Firmware:
//    - wpisuje wartości i kolory do komponentów o ustalonych nazwach
//      (t_cur1.txt=..., t_mode1.bco=..., j_pwr1.val=...)
//    - przełącza strony komendą "page N"
//    - odbiera dotyk jako printh 65 <strona> <ID> 01 (ID z ui.h)
//  Lista nazw i ID: docs/DISPLAY.md
// ============================================================

#define UI_REFRESH_MS       150     // odświeżanie bieżącej strony (tylko zmiany)
#define UI_LINK_CHECK_MS    2000    // "sendme" - kontrola połączenia
#define UI_LINK_MAX_MISSES  3
#define UI_REINIT_HOLDOFF_MS 60000
#define UI_STATUS_MS        1500
#define AUTOSAVE_DELAY_MS   20000
#define DIM_IDLE            15      // jasność gdy wszystko śpi

static uint8_t  g_page            = PAGE_MAIN;
static uint8_t  g_pid_ch          = 0;          // kanał edytowany na stronie PID
static uint32_t g_last_refresh    = 0;
static uint32_t g_last_link_check = 0;
static uint8_t  g_link_misses     = 0;
static uint8_t  g_link_seen       = 0;
static uint8_t  g_sendme_pending  = 0;
static uint32_t g_last_reinit     = 0;
static int      g_last_dim        = -1;
static char     g_status[24]      = "";
static uint32_t g_status_until    = 0;

// ============================================================
//  Komponenty Nextion (wysyłane tylko zmiany - cache w nextion.c)
// ============================================================
#define KEEP    (-1)    // nie zmieniaj koloru - zostaje ten z edytora

// Tekst; kolory tylko tam, gdzie niosą informację (reszta jak w HMI)
static void w_text(const char *obj, const char *txt, int32_t pco, int32_t bco) {
    nextion_set_txt(obj, txt);
    if (pco != KEEP) nextion_set_attr(obj, "pco", pco);
    if (bco != KEEP) nextion_set_attr(obj, "bco", bco);
}

// Komponent Number
static void w_num(const char *obj, int value) {
    nextion_set_val(obj, value);
}

// Progress bar: wartość 0-100 + kolor paska
static void w_progress(const char *obj, int percent, uint16_t pco) {
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    nextion_set_val(obj, percent);
    nextion_set_attr(obj, "pco", pco);
}

static void set_page(uint8_t page) {
    g_page = page;
    nextion_goto_page(page);        // czyści też cache
    g_last_refresh = 0;
}

// ============================================================
//  Pomocnicze
// ============================================================
void ui_status(const char *msg, uint32_t now_ms) {
    strncpy(g_status, msg, sizeof(g_status) - 1);
    g_status[sizeof(g_status) - 1] = '\0';
    g_status_until = now_ms + UI_STATUS_MS;
}

void ui_mark_dirty(SystemState_t *sys, uint32_t now_ms) {
    sys->config_dirty   = 1;
    sys->dirty_since_ms = now_ms;
}

void ui_save_now(SystemState_t *sys, uint32_t now_ms) {
    channels_store_to_config(sys);
    config_save(&sys->config);
    sys->config_dirty = 0;
    ui_status("ZAPISANO", now_ms);
    printf("[CFG] zapisano do flash\n");
}

void ui_select_channel(SystemState_t *sys, int ch, uint32_t now_ms) {
    if (ch < 0 || ch >= NUM_CHANNELS) return;
    if (sys->selected_ch != ch) {
        sys->selected_ch = (uint8_t)ch;
        ui_mark_dirty(sys, now_ms);
    }
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static float stepf(float v, float step, float lo, float hi) {
    v += step;
    v = roundf(v / fabsf(step)) * fabsf(step);   // bez narastania błędu float
    return v < lo ? lo : (v > hi ? hi : v);
}

// ============================================================
//  Opis stanu kanału
// ============================================================
static void mode_style(const Channel_t *ch, const char **txt, uint16_t *bco, uint16_t *pco) {
    *pco = CLR_BLACK;
    switch (ch->fault) {
        case FAULT_NO_TIP:   *txt = "BRAK KOLBY";  *bco = CLR_GRAY; *pco = CLR_TEXT; return;
        case FAULT_OVERTEMP: *txt = "PRZEGRZANIE"; *bco = CLR_RED;  *pco = CLR_TEXT; return;
        case FAULT_RUNAWAY:  *txt = "BLAD GRZANIA";*bco = CLR_RED;  *pco = CLR_TEXT; return;
        default: break;
    }
    switch (ch->mode) {
        case MODE_ACTIVE:    *txt = "PRACA";      *bco = CLR_GREEN;  break;
        case MODE_STANDBY:   *txt = "CZUWANIE";   *bco = CLR_ORANGE; break;
        case MODE_HIBERNATE: *txt = "HIBERNACJA"; *bco = CLR_BLUE;   *pco = CLR_TEXT; break;
        case MODE_EXCHANGE:  *txt = "WYM. GROTU"; *bco = CLR_YELLOW; break;
        case MODE_OFF:
        default:             *txt = "WYLACZONA";  *bco = CLR_GRAY;   *pco = CLR_TEXT; break;
    }
}

static uint16_t temp_color(const Channel_t *ch, const Config_t *cfg) {
    if (ch->fault == FAULT_OVERTEMP || ch->fault == FAULT_RUNAWAY) return CLR_RED;
    if (ch->fault == FAULT_NO_TIP || ch->mode == MODE_OFF)         return CLR_GRAY;
    float diff = ch->temp_current - (float)channel_target_temp(ch, cfg);
    if (diff < -10.0f) return CLR_ORANGE;   // nagrzewa się
    if (diff >  10.0f) return CLR_BLUE;     // stygnie
    return CLR_TEXT;                        // w zadanej temperaturze
}

// ============================================================
//  STRONA 0 - główna
// ============================================================
static void refresh_main(const SystemState_t *sys) {
    char buf[24], obj[12];

    snprintf(buf, sizeof(buf), "%.1fV", sys->v_input);
    w_text("t_vin", buf, sys->v_input < VIN_LOW_WARN ? CLR_RED : CLR_TEXT_DIM, KEEP);

    for (int i = 0; i < NUM_CHANNELS; i++) {
        const Channel_t *ch = &sys->ch[i];
        int n = i + 1;
        bool sel = (sys->selected_ch == i);

#if NUM_CHANNELS > 1
        // Belka z nazwą = wybrana kolba (tylko HMI na 2 kolby)
        snprintf(obj, sizeof(obj), "t_name%d", n);
        nextion_set_attr(obj, "bco", sel ? CLR_ACCENT : CLR_PANEL_HDR);
        nextion_set_attr(obj, "pco", sel ? CLR_BLACK  : CLR_TEXT_DIM);
#else
        (void)sel;
#endif

        snprintf(obj, sizeof(obj), "t_cur%d", n);
        if (ch->fault == FAULT_NO_TIP) snprintf(buf, sizeof(buf), "---");
        else                           snprintf(buf, sizeof(buf), "%d", (int)(ch->temp_current + 0.5f));
        w_text(obj, buf, temp_color(ch, &sys->config), KEEP);

        snprintf(obj, sizeof(obj), "t_set%d", n);
        if (ch->mode == MODE_HIBERNATE)
            snprintf(buf, sizeof(buf), "SET %d (%d)", channel_target_temp(ch, &sys->config), ch->setpoint);
        else
            snprintf(buf, sizeof(buf), "SET %d", ch->setpoint);
        w_text(obj, buf, KEEP, KEEP);

        snprintf(obj, sizeof(obj), "j_pwr%d", n);
        w_progress(obj, (int)(ch->power_percent + 0.5f),
                   ch->power_percent > 90.0f ? CLR_ORANGE : CLR_ACCENT);

        const char *mtxt; uint16_t mbco, mpco;
        mode_style(ch, &mtxt, &mbco, &mpco);
        snprintf(obj, sizeof(obj), "t_mode%d", n);
        w_text(obj, mtxt, mpco, mbco);
    }

    // Przyciski profili - dotyczą wybranej kolby
    const Channel_t *sel = &sys->ch[sys->selected_ch];
    for (int p = 0; p < NUM_PROFILES; p++) {
        snprintf(obj, sizeof(obj), "b_p%d", p + 1);
        snprintf(buf, sizeof(buf), "%d", sys->config.profile_temp[p]);
        bool act = (sel->active_profile == p);
        // Aktywny profil: czerwona liczba (dobrze widoczna na ciemnym przycisku)
        w_text(obj, buf, act ? CLR_RED : CLR_TEXT, CLR_BTN);
    }
}

static void handle_main(SystemState_t *sys, uint8_t id, uint32_t now_ms) {
    Channel_t *sel = &sys->ch[sys->selected_ch];
    switch (id) {
        case EV_MAIN_P1: case EV_MAIN_P2: case EV_MAIN_P3:
            channel_set_profile(sel, &sys->config, id - EV_MAIN_P1);
            ui_mark_dirty(sys, now_ms);
            break;
        case EV_MAIN_MINUS:
            channel_adjust_setpoint(sel, &sys->config, -(int)sys->config.temp_step);
            ui_mark_dirty(sys, now_ms);
            break;
        case EV_MAIN_PLUS:
            channel_adjust_setpoint(sel, &sys->config, +(int)sys->config.temp_step);
            ui_mark_dirty(sys, now_ms);
            break;
        case EV_MAIN_SETTINGS:
            set_page(PAGE_SETTINGS);
            break;
        case EV_MAIN_SEL_CH1:
        case EV_MAIN_SEL_CH2:
            ui_select_channel(sys, id - EV_MAIN_SEL_CH1, now_ms);
            break;
        case EV_MAIN_PWR_CH1:
        case EV_MAIN_PWR_CH2: {
            int c = id - EV_MAIN_PWR_CH1;
            if (c < NUM_CHANNELS) {
                ui_select_channel(sys, c, now_ms);
                channel_toggle_power(&sys->ch[c]);
            }
            break;
        }
    }
}

// ============================================================
//  STRONA 1 - ustawienia ogólne
// ============================================================
static void refresh_settings(const SystemState_t *sys) {
    const Config_t *cfg = &sys->config;
    char obj[12];
    for (int p = 0; p < NUM_PROFILES; p++) {
        snprintf(obj, sizeof(obj), "n_prof%d", p + 1);
        w_num(obj, cfg->profile_temp[p]);
    }
    w_num("n_sleep", (int)(cfg->standby_delay_ms / 1000));
    w_num("n_hib",   cfg->hibernate_temp);
    w_num("n_aoff",  cfg->auto_off_min);
    w_text("t_status", g_status, KEEP, KEEP);

    // Podgląd PID wybranej kolby (jeśli na tej stronie są pola t_kp/t_ki/t_kd)
    const Channel_t *ch = &sys->ch[sys->selected_ch];
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1f", ch->pid.kp);  w_text("t_kp", buf, KEEP, KEEP);
    snprintf(buf, sizeof(buf), "%.2f", ch->pid.ki);  w_text("t_ki", buf, KEEP, KEEP);
    snprintf(buf, sizeof(buf), "%.1f", ch->pid.kd);  w_text("t_kd", buf, KEEP, KEEP);
}

static void handle_settings(SystemState_t *sys, uint8_t id, uint32_t now_ms) {
    Config_t *cfg = &sys->config;
    bool changed = true;

    switch (id) {
        case EV_SET_P1_UP: case EV_SET_P1_DN:
        case EV_SET_P2_UP: case EV_SET_P2_DN:
        case EV_SET_P3_UP: case EV_SET_P3_DN: {
            int p     = (id - 1) / 2;
            int delta = (id & 1) ? cfg->temp_step : -(int)cfg->temp_step;
            cfg->profile_temp[p] = (uint16_t)clampi(cfg->profile_temp[p] + delta,
                                                    TEMP_SET_MIN, cfg->temp_max);
            // Kanały pracujące na tym profilu dostają nową nastawę od razu
            for (int c = 0; c < NUM_CHANNELS; c++) {
                if (sys->ch[c].active_profile == p) sys->ch[c].setpoint = cfg->profile_temp[p];
            }
            break;
        }
        case EV_SET_SLEEP_UP: case EV_SET_SLEEP_DN: {
            int s = (int)(cfg->standby_delay_ms / 1000) +
                    ((id == EV_SET_SLEEP_UP) ? STANDBY_DELAY_STEP_S : -STANDBY_DELAY_STEP_S);
            cfg->standby_delay_ms = (uint32_t)clampi(s, STANDBY_DELAY_MIN_S, STANDBY_DELAY_MAX_S) * 1000u;
            break;
        }
        case EV_SET_HIB_UP: case EV_SET_HIB_DN:
            cfg->hibernate_temp = (uint16_t)clampi(cfg->hibernate_temp +
                ((id == EV_SET_HIB_UP) ? TEMP_STEP : -TEMP_STEP), HIBERNATE_TEMP_MIN, HIBERNATE_TEMP_MAX);
            break;
        case EV_SET_AOFF_UP: case EV_SET_AOFF_DN:
            cfg->auto_off_min = (uint16_t)clampi(cfg->auto_off_min +
                ((id == EV_SET_AOFF_UP) ? AUTO_OFF_STEP_MIN : -AUTO_OFF_STEP_MIN), 0, AUTO_OFF_MAX_MIN);
            break;
        case EV_SET_SYS_PAGE:
            set_page(PAGE_SYSTEM);
            changed = false;
            break;
        case EV_SET_SAVE:
            ui_save_now(sys, now_ms);
            changed = false;
            break;
        case EV_SET_BACK:
            set_page(PAGE_MAIN);
            changed = false;
            break;
        default:
            changed = false;
            break;
    }
    if (changed) ui_mark_dirty(sys, now_ms);
}

// ============================================================
//  Wspólne: podgląd temperatury i mocy (strojenie, kalibracja)
// ============================================================
static void live_text(const Channel_t *ch, const Config_t *cfg, char *buf, size_t n) {
    if (ch->fault == FAULT_NO_TIP)
        snprintf(buf, n, "T=--- P=0%%");
    else
        snprintf(buf, n, "T=%d/%d P=%d%%", (int)(ch->temp_current + 0.5f),
                 channel_target_temp(ch, cfg), (int)(ch->power_percent + 0.5f));
}

// ============================================================
//  STRONA 2 - system: korekta, limit mocy, jasność, krok, max temp., licznik
// ============================================================
static void refresh_system(const SystemState_t *sys) {
    const Config_t  *cfg = &sys->config;
    const Channel_t *ch  = &sys->ch[sys->selected_ch];
    char buf[24];

    snprintf(buf, sizeof(buf), "%+d", cfg->ch[sys->selected_ch].temp_offset);
    w_text("t_off", buf, KEEP, KEEP);
    w_num("n_pwr",  cfg->power_limit);
    w_num("n_bri",  cfg->brightness);
    w_num("n_step", cfg->temp_step);
    w_num("n_tmax", cfg->temp_max);

    uint32_t s = cfg->ch[sys->selected_ch].work_s;
    snprintf(buf, sizeof(buf), "%lu h %02lu min", (unsigned long)(s / 3600),
             (unsigned long)((s / 60) % 60));
    w_text("t_hours", buf, KEEP, KEEP);

    live_text(ch, cfg, buf, sizeof(buf));
    w_text("t_live", buf, KEEP, KEEP);
    w_text("t_status", g_status, KEEP, KEEP);
}

// Obniżenie maks. temperatury ogranicza profile i bieżące nastawy
static void apply_temp_max(SystemState_t *sys) {
    Config_t *cfg = &sys->config;
    for (int p = 0; p < NUM_PROFILES; p++) {
        if (cfg->profile_temp[p] > cfg->temp_max) cfg->profile_temp[p] = cfg->temp_max;
    }
    for (int c = 0; c < NUM_CHANNELS; c++) {
        if (sys->ch[c].setpoint > cfg->temp_max) sys->ch[c].setpoint = cfg->temp_max;
    }
}

static void handle_system(SystemState_t *sys, uint8_t id, uint32_t now_ms) {
    Config_t        *cfg = &sys->config;
    ChannelConfig_t *cc  = &cfg->ch[sys->selected_ch];
    bool changed = true;

    switch (id) {
        case EV_SYS_OFF_UP:
            cc->temp_offset = (int8_t)clampi(cc->temp_offset + 1, -TEMP_OFFSET_LIMIT, TEMP_OFFSET_LIMIT);
            break;
        case EV_SYS_OFF_DN:
            cc->temp_offset = (int8_t)clampi(cc->temp_offset - 1, -TEMP_OFFSET_LIMIT, TEMP_OFFSET_LIMIT);
            break;
        case EV_SYS_PWR_UP:
            cfg->power_limit = (uint8_t)clampi(cfg->power_limit + POWER_LIMIT_STEP, POWER_LIMIT_MIN, 100);
            break;
        case EV_SYS_PWR_DN:
            cfg->power_limit = (uint8_t)clampi(cfg->power_limit - POWER_LIMIT_STEP, POWER_LIMIT_MIN, 100);
            break;
        case EV_SYS_BRI_UP:
            cfg->brightness = (uint8_t)clampi(cfg->brightness + BRIGHTNESS_STEP, 10, 100);
            break;
        case EV_SYS_BRI_DN:
            cfg->brightness = (uint8_t)clampi(cfg->brightness - BRIGHTNESS_STEP, 10, 100);
            break;
        case EV_SYS_STEP_UP:
            cfg->temp_step = config_temp_step_next(cfg->temp_step, +1);
            break;
        case EV_SYS_STEP_DN:
            cfg->temp_step = config_temp_step_next(cfg->temp_step, -1);
            break;
        case EV_SYS_TMAX_UP:
            cfg->temp_max = (uint16_t)clampi(cfg->temp_max + TEMP_MAX_STEP, TEMP_MAX_MIN, TEMP_SET_MAX);
            break;
        case EV_SYS_TMAX_DN:
            cfg->temp_max = (uint16_t)clampi(cfg->temp_max - TEMP_MAX_STEP, TEMP_MAX_MIN, TEMP_SET_MAX);
            apply_temp_max(sys);
            break;
        case EV_SYS_PID_PAGE:
            g_pid_ch = sys->selected_ch;
            set_page(PAGE_PID);
            changed = false;
            break;
        case EV_SYS_SAVE:
            ui_save_now(sys, now_ms);
            changed = false;
            break;
        case EV_SYS_BACK:
            set_page(PAGE_SETTINGS);
            changed = false;
            break;
        default:
            changed = false;
            break;
    }
    if (changed) ui_mark_dirty(sys, now_ms);
}

// ============================================================
//  STRONA 3 - PID
// ============================================================
static void refresh_pid(const SystemState_t *sys) {
    const Channel_t *ch = &sys->ch[g_pid_ch];
    char buf[24];

    snprintf(buf, sizeof(buf), "KOLBA %d", g_pid_ch + 1);
    w_text("t_ch", buf, KEEP, KEEP);
    snprintf(buf, sizeof(buf), "%.1f", ch->pid.kp);  w_text("t_kp", buf, KEEP, KEEP);
    snprintf(buf, sizeof(buf), "%.2f", ch->pid.ki);  w_text("t_ki", buf, KEEP, KEEP);
    snprintf(buf, sizeof(buf), "%.1f", ch->pid.kd);  w_text("t_kd", buf, KEEP, KEEP);

    live_text(ch, &sys->config, buf, sizeof(buf));
    w_text("t_live", buf, KEEP, KEEP);
    w_text("t_status", g_status, KEEP, KEEP);
}

static void handle_pid(SystemState_t *sys, uint8_t id, uint32_t now_ms) {
    Channel_t       *ch = &sys->ch[g_pid_ch];
    ChannelConfig_t *cc = &sys->config.ch[g_pid_ch];
    bool changed = true;

    switch (id) {
        case EV_PID_KP_UP: ch->pid.kp = stepf(ch->pid.kp, +1.0f,  0.0f, 200.0f); break;
        case EV_PID_KP_DN: ch->pid.kp = stepf(ch->pid.kp, -1.0f,  0.0f, 200.0f); break;
        case EV_PID_KI_UP: ch->pid.ki = stepf(ch->pid.ki, +0.05f, 0.0f, 20.0f);  break;
        case EV_PID_KI_DN: ch->pid.ki = stepf(ch->pid.ki, -0.05f, 0.0f, 20.0f);  break;
        case EV_PID_KD_UP: ch->pid.kd = stepf(ch->pid.kd, +0.5f,  0.0f, 100.0f); break;
        case EV_PID_KD_DN: ch->pid.kd = stepf(ch->pid.kd, -0.5f,  0.0f, 100.0f); break;
        case EV_PID_CH_SWITCH:
            g_pid_ch = (uint8_t)((g_pid_ch + 1) % NUM_CHANNELS);
            changed = false;
            break;
        case EV_PID_DEFAULTS:
            config_pid_defaults(cc);
            channel_apply_pid_gains(ch, cc);
            pid_reset(&ch->pid);
            ui_status("DOMYSLNE", now_ms);
            break;
        case EV_PID_SAVE:
            ui_save_now(sys, now_ms);
            changed = false;
            break;
        case EV_PID_BACK:
            set_page(PAGE_SYSTEM);
            changed = false;
            break;
        default:
            changed = false;
            break;
    }
    if (changed) ui_mark_dirty(sys, now_ms);
}

// ============================================================
//  Jasność: przygaś gdy żadna kolba nie pracuje
// ============================================================
static void update_dim(const SystemState_t *sys) {
    // Przygaszamy tylko, gdy każda kolba śpi (hibernacja) albo jest wyłączona.
    // Brak kolby / błąd = ekran jasny, żeby komunikat był dobrze widoczny.
    bool idle = true;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        const Channel_t *ch = &sys->ch[i];
        bool sleeping = (ch->mode == MODE_HIBERNATE || ch->mode == MODE_OFF);
        if (!sleeping || ch->fault != FAULT_NONE) {
            idle = false;
        }
    }
    (void)idle;                     // od v2.3 bez przygaszania - zawsze jasność z ustawień
    int dim = sys->config.brightness;
    if (dim != g_last_dim) {
        g_last_dim = dim;
        nextion_cmdf("dim=%d", dim);
    }
}

static void refresh_all(SystemState_t *sys) {
    switch (g_page) {
        case PAGE_MAIN:     refresh_main(sys);     break;
        case PAGE_SETTINGS: refresh_settings(sys); break;
        case PAGE_SYSTEM:   refresh_system(sys);   break;
        case PAGE_PID:      refresh_pid(sys);      break;
        default: break;
    }
    update_dim(sys);
}

// ============================================================
//  Dotyk: printh 65 <strona> <ID> 01
// ============================================================
static void handle_touch(SystemState_t *sys, const NextionEvent_t *ev, uint32_t now_ms) {
    printf("[TOUCH] %lu ms: strona %u id %u %s\n", (unsigned long)now_ms, ev->page, ev->comp,
           ev->press ? "wcisniety" : "puszczony");
    if (!ev->press) return;
    if (ev->page != g_page) {           // strona zmieniona po stronie HMI
        g_page = ev->page;
        nextion_cache_clear();
    }
    if      (ev->page == PAGE_MAIN)     handle_main(sys, ev->comp, now_ms);
    else if (ev->page == PAGE_SETTINGS) handle_settings(sys, ev->comp, now_ms);
    else if (ev->page == PAGE_SYSTEM)   handle_system(sys, ev->comp, now_ms);
    else if (ev->page == PAGE_PID)      handle_pid(sys, ev->comp, now_ms);
    g_last_refresh = 0;                 // odśwież od razu
}

// ============================================================
//  API
// ============================================================
static void display_setup(void) {
    g_last_dim = -1;
}

void ui_init(SystemState_t *sys) {
    nextion_init();
    display_setup();
    set_page(PAGE_MAIN);
    refresh_all(sys);
}

static void reinit_display(SystemState_t *sys, uint32_t now_ms) {
    printf("[UI] ponowna inicjalizacja wyswietlacza\n");
    heaters_all_off();          // nextion_init blokuje do ~2 s
    g_last_reinit  = now_ms;
    g_link_misses  = 0;
    g_sendme_pending = 0;
    ui_init(sys);
}

void ui_task(SystemState_t *sys, uint32_t now_ms) {
    NextionEvent_t ev;
    while (nextion_poll_event(&ev)) {
        switch (ev.type) {
            case NX_EV_TOUCH:
                g_link_seen = 1;
                handle_touch(sys, &ev, now_ms);
                break;
            case NX_EV_PAGE:                // odpowiedź na sendme = łącze działa
                g_link_seen = 1;
                g_link_misses = 0;
                g_sendme_pending = 0;
                if (ev.page != g_page) {    // strona zmieniona po stronie HMI
                    g_page = ev.page;
                    nextion_cache_clear();
                }
                break;
            case NX_EV_RESET:
                printf("[UI] wyswietlacz zrestartowany\n");
                display_setup();
                set_page(g_page);
                break;
            default:
                break;
        }
    }

    // Kontrola łącza: sendme -> 0x66
    if ((now_ms - g_last_link_check) >= UI_LINK_CHECK_MS) {
        g_last_link_check = now_ms;
        if (g_sendme_pending && g_link_seen) {
            if (++g_link_misses >= UI_LINK_MAX_MISSES &&
                (g_last_reinit == 0 || (now_ms - g_last_reinit) >= UI_REINIT_HOLDOFF_MS)) {
                reinit_display(sys, now_ms);
                return;
            }
        }
        nextion_send_cmd("sendme");
        g_sendme_pending = 1;
    }

    if (g_status[0] && (int32_t)(now_ms - g_status_until) >= 0) g_status[0] = '\0';

    if ((now_ms - g_last_refresh) >= UI_REFRESH_MS) {
        g_last_refresh = now_ms;
        refresh_all(sys);
    }

    // Autozapis po chwili bezczynności
    if (sys->config_dirty && (now_ms - sys->dirty_since_ms) >= AUTOSAVE_DELAY_MS) {
        ui_save_now(sys, now_ms);
        g_status[0] = '\0';     // bez komunikatu przy autozapisie
    }
}
