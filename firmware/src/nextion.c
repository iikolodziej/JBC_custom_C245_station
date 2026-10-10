#include "nextion.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define NEXTION_PROBE_MS    250

static uint32_t g_baud    = 9600;
static bool     g_link_ok = false;

// ============================================================
//  UART
// ============================================================
static void uart_configure(uint32_t baud) {
    uart_deinit(UART_NEXTION);
    uart_init(UART_NEXTION, baud);
    gpio_set_function(PIN_NEXTION_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_NEXTION_RX, GPIO_FUNC_UART);
    gpio_pull_up(PIN_NEXTION_RX);   // UART w spoczynku = stan wysoki; odłączony ekran nie daje śmieci
    uart_set_hw_flow(UART_NEXTION, false, false);
    uart_set_format(UART_NEXTION, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_NEXTION, true);
    g_baud = baud;
}

// Diagnostyka odbioru (komenda "nx" w konsoli USB)
static uint32_t g_rx_bytes = 0;
static uint32_t g_rx_errors = 0;        // błędy ramki/parzystości/przepełnienia
static uint8_t  g_rx_last[16];
static uint8_t  g_rx_last_n = 0;

// Odczyt bajtu z zapisem statystyk; bity 8-11 rejestru DR = błędy odbioru
static uint8_t nx_getc(void) {
    while (!uart_is_readable(UART_NEXTION)) tight_loop_contents();
    uint32_t dr = uart_get_hw(UART_NEXTION)->dr;
    uint8_t c = (uint8_t)(dr & 0xFF);
    g_rx_bytes++;
    if (dr & 0xF00) g_rx_errors++;
    if (g_rx_last_n < sizeof(g_rx_last)) {
        g_rx_last[g_rx_last_n++] = c;
    } else {
        memmove(g_rx_last, g_rx_last + 1, sizeof(g_rx_last) - 1);
        g_rx_last[sizeof(g_rx_last) - 1] = c;
    }
    return c;
}

void nextion_debug_print(void) {
    int level = gpio_get(PIN_NEXTION_RX) ? 1 : 0;
    // Test pinu RX: z podciąganiem w dół i w górę.
    //  0/1 = pin wisi w powietrzu (nic go nie steruje)
    //  0/0 = coś ściąga go do masy,  1/1 = wyświetlacz trzyma stan wysoki (poprawnie)
    gpio_pull_down(PIN_NEXTION_RX);
    sleep_ms(2);
    int with_down = gpio_get(PIN_NEXTION_RX) ? 1 : 0;
    gpio_pull_up(PIN_NEXTION_RX);
    sleep_ms(2);
    int with_up = gpio_get(PIN_NEXTION_RX) ? 1 : 0;
    printf("NX: link=%s baud=%lu RX(GP%d)=%d test pull-down/up=%d/%d bajty=%lu bledy=%lu ostatnie:",
           g_link_ok ? "OK" : "brak", (unsigned long)g_baud, PIN_NEXTION_RX, level,
           with_down, with_up, (unsigned long)g_rx_bytes, (unsigned long)g_rx_errors);
    for (int i = 0; i < g_rx_last_n; i++) printf(" %02X", g_rx_last[i]);
    printf("\n");
}

static void send_terminator(void) {
    uart_putc_raw(UART_NEXTION, 0xFF);
    uart_putc_raw(UART_NEXTION, 0xFF);
    uart_putc_raw(UART_NEXTION, 0xFF);
}

static void flush_rx(void) {
    while (uart_is_readable(UART_NEXTION)) (void)nx_getc();
}

void nextion_send_cmd(const char *cmd) {
    if (cmd == NULL || cmd[0] == '\0') return;
    uart_puts(UART_NEXTION, cmd);
    send_terminator();
}

void nextion_cmdf(const char *fmt, ...) {
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    nextion_send_cmd(buf);
}

// ============================================================
//  Inicjalizacja z autodetekcją prędkości
// ============================================================
static int read_frame_blocking(uint8_t *buf, int buf_size, uint32_t timeout_ms) {
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    int len = 0;
    while (absolute_time_diff_us(get_absolute_time(), deadline) > 0) {
        while (uart_is_readable(UART_NEXTION)) {
            uint8_t c = nx_getc();
            if (len < buf_size) buf[len++] = c;
            if (len >= 3 && buf[len-1] == 0xFF && buf[len-2] == 0xFF && buf[len-3] == 0xFF) {
                return len - 3;
            }
        }
        sleep_ms(1);
        watchdog_update();      // autodetekcja trwa do ~2 s
    }
    return 0;
}

static bool try_connect(uint32_t baud) {
    uint8_t frame[96];
    uart_configure(baud);
    sleep_ms(20);
    send_terminator();              // wyczyść ewentualne śmieci w buforze Nextion
    uart_tx_wait_blocking(UART_NEXTION);
    sleep_ms(20);
    flush_rx();

    nextion_send_cmd("connect");
    uart_tx_wait_blocking(UART_NEXTION);

    // Może przyjść ramka błędu (0x1A) przed comok - czytaj do skutku
    absolute_time_t deadline = make_timeout_time_ms(NEXTION_PROBE_MS);
    while (absolute_time_diff_us(get_absolute_time(), deadline) > 0) {
        int len = read_frame_blocking(frame, sizeof(frame) - 1, NEXTION_PROBE_MS);
        if (len <= 0) break;
        frame[len] = '\0';
        if (strstr((const char *)frame, "comok") != NULL) {
            printf("[NEXTION] %lu baud: %s\n", (unsigned long)baud, (const char *)frame);
            return true;
        }
    }
    return false;
}

void nextion_init(void) {
    static const uint32_t candidates[] = {NEXTION_BAUD_RUN, 9600, 115200, 57600, 38400, 19200};
    g_link_ok = false;

    sleep_ms(300);  // Nextion potrzebuje chwili na start

    uint32_t found = 0;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]) && !found; i++) {
        if (try_connect(candidates[i])) found = candidates[i];
    }

    if (found && found != NEXTION_BAUD_RUN) {
        nextion_cmdf("baud=%lu", (unsigned long)NEXTION_BAUD_RUN);
        uart_tx_wait_blocking(UART_NEXTION);
        sleep_ms(100);
        if (try_connect(NEXTION_BAUD_RUN)) {
            found = NEXTION_BAUD_RUN;
        } else {
            uart_configure(found);  // zostań na starej prędkości
        }
    }

    if (found) {
        g_link_ok = true;
        uart_configure(found);
    } else {
        // Brak odpowiedzi (np. odłączone RX) - ustaw na ślepo 115200 na każdej prędkości
        for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
            uart_configure(candidates[i]);
            send_terminator();
            nextion_cmdf("baud=%lu", (unsigned long)NEXTION_BAUD_RUN);
            uart_tx_wait_blocking(UART_NEXTION);
            sleep_ms(30);
        }
        uart_configure(NEXTION_BAUD_RUN);
        printf("[NEXTION] Brak odpowiedzi na 'connect' - tryb tylko TX @ %lu\n",
               (unsigned long)NEXTION_BAUD_RUN);
    }

    nextion_send_cmd("bkcmd=0");    // bez ramek potwierdzeń
    nextion_cache_clear();
    sleep_ms(20);
    flush_rx();
}

bool nextion_link_ok(void) { return g_link_ok; }

// ============================================================
//  Cache wysłanych wartości - wysyłamy tylko zmiany
// ============================================================
#define CACHE_SIZE  64
#define CACHE_KEY   20

typedef struct {
    char     key[CACHE_KEY];    // np. "t_cur1.txt"
    uint32_t hash;
} CacheEntry_t;

static CacheEntry_t g_cache[CACHE_SIZE];
static int          g_cache_n = 0;

void nextion_cache_clear(void) { g_cache_n = 0; }

static uint32_t fnv1a(const char *s, uint32_t h) {
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

// true = wartość się zmieniła (trzeba wysłać)
static bool cache_update(const char *obj, const char *attr, uint32_t hash) {
    char key[CACHE_KEY];
    snprintf(key, sizeof(key), "%s.%s", obj, attr);
    for (int i = 0; i < g_cache_n; i++) {
        if (strcmp(g_cache[i].key, key) == 0) {
            if (g_cache[i].hash == hash) return false;
            g_cache[i].hash = hash;
            return true;
        }
    }
    if (g_cache_n < CACHE_SIZE) {
        memcpy(g_cache[g_cache_n].key, key, CACHE_KEY);    // snprintf gwarantuje '\0'
        g_cache[g_cache_n].hash = hash;
        g_cache_n++;
    }
    return true;
}

bool nextion_cache_changed(const char *key, uint32_t hash) {
    return cache_update(key, "", hash);
}

uint32_t nextion_hash(const char *s, uint32_t seed) {
    return fnv1a(s, seed ^ 2166136261u);
}

void nextion_set_txt(const char *obj, const char *txt) {
    if (!cache_update(obj, "txt", fnv1a(txt, 2166136261u))) return;
    nextion_cmdf("%s.txt=\"%s\"", obj, txt);
}

void nextion_set_attr(const char *obj, const char *attr, int32_t val) {
    if (!cache_update(obj, attr, (uint32_t)val ^ 0x5A5A5A5Au)) return;
    nextion_cmdf("%s.%s=%ld", obj, attr, (long)val);
}

void nextion_set_val(const char *obj, int32_t val) {
    nextion_set_attr(obj, "val", val);
}

void nextion_goto_page(uint8_t page) {
    nextion_cmdf("page %u", page);
    nextion_cache_clear();
}

// ============================================================
//  Odbiór ramek (nieblokujący)
// ============================================================
#define RX_BUF_SIZE 32
static uint8_t rx_buf[RX_BUF_SIZE];
static int     rx_len = 0;

bool nextion_poll_event(NextionEvent_t *ev) {
    ev->type = NX_EV_NONE;

    while (uart_is_readable(UART_NEXTION)) {
        uint8_t c = nx_getc();
        if (rx_len >= RX_BUF_SIZE) rx_len = 0;     // śmieci - zacznij od nowa
        rx_buf[rx_len++] = c;

        // Dotyk z "printh 65 PP ID 01" przychodzi BEZ końcówki FF FF FF - obsłuż
        // go od razu po 4 bajtach. Końcówka z "Send Component ID" (jeśli jest)
        // da potem pustą ramkę, która jest pomijana.
        if (rx_len == 4 && rx_buf[0] == 0x65) {
            rx_len = 0;
            g_link_ok = true;
            ev->type  = NX_EV_TOUCH;
            ev->page  = rx_buf[1];
            ev->comp  = rx_buf[2];
            ev->press = rx_buf[3];
            return true;
        }

        if (rx_len < 3|| rx_buf[rx_len-1] != 0xFF || rx_buf[rx_len-2] != 0xFF ||
            rx_buf[rx_len-3] != 0xFF) {
            continue;
        }

        int len = rx_len - 3;
        rx_len = 0;
        if (len <= 0) continue;

        g_link_ok = true;
        switch (rx_buf[0]) {
            case 0x65:
                if (len >= 4) {
                    ev->type  = NX_EV_TOUCH;
                    ev->page  = rx_buf[1];
                    ev->comp  = rx_buf[2];
                    ev->press = rx_buf[3];
                    return true;
                }
                break;
            case 0x66:
                if (len >= 2) {
                    ev->type = NX_EV_PAGE;
                    ev->page = rx_buf[1];
                    return true;
                }
                break;
            case 0x67:
                if (len >= 6) {
                    ev->type  = NX_EV_XY;
                    ev->x     = (uint16_t)((rx_buf[1] << 8) | rx_buf[2]);
                    ev->y     = (uint16_t)((rx_buf[3] << 8) | rx_buf[4]);
                    ev->press = rx_buf[5];
                    return true;
                }
                break;
            case 0x88:
                ev->type = NX_EV_RESET;
                return true;
            case 0x00:
                if (len >= 3 && rx_buf[1] == 0x00 && rx_buf[2] == 0x00) {
                    ev->type = NX_EV_RESET;
                    return true;
                }
                break;
            default:
                break;      // 0x1A itp. - ignoruj
        }
    }
    return false;
}
