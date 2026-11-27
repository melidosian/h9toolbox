/* Far audio: the audio hardware as the working radio leaves it. Runs from
 * main flash 0xEA000 (Back-hold launch on H9 Toolbox Firmware with the far
 * launcher), so the radio app has started and key beeps play. Doom's audio
 * probe shows the same registers inside the Doom firmware, where the same
 * stock tone is silent; the difference is what Doom has to set up.
 *
 * Pages (key 2 cycles), each shown live and as a snapshot taken 60 ms into
 * a stock tone (key 7 plays stock tone 0, stock app+0x5231A):
 *   D  DAC block 0x1E2F00: +0 CON, +4 ADR, +8 LEN, +10 HRP, +14 SWP, +28 VOL, +2C
 *   A  second audio block 0x1E19C0 +0..+14
 *   G  GPIO ports A-D (0x1E5000 + 0x40 n): +0 OUT and +8 DIR
 * Key 4 switches between live and snapshot (S on the title line).
 *
 * Freestanding: no globals, no library calls (the build checks). */
#include <stdint.h>

void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1);
void h9_lcd_write(const void *buffer, unsigned length);
unsigned h9_key(void);
void h9_delay(unsigned units);
void h9_led(unsigned on);
void h9_play_tone(unsigned n); /* stock app+0x5231A */

enum { WIDTH = 160, HEIGHT = 128, CHUNK = 128, ROWS = 8 };
enum { WHITE = 0xffff, BLACK = 0x0000, YELLOW = 0xffe0, GREEN = 0x07e0, CYAN = 0x07ff };
enum { KEY_2 = 4, KEY_4 = 6, KEY_7 = 9 };

static void fill(unsigned x0, unsigned y0, unsigned w, unsigned h, uint16_t color)
{
    uint8_t buffer[CHUNK];
    for (unsigned i = 0; i < CHUNK; i += 2) {
        buffer[i] = (uint8_t)(color >> 8);
        buffer[i + 1] = (uint8_t)color;
    }
    h9_lcd_window(x0, y0, x0 + w - 1, y0 + h - 1);
    for (unsigned left = w * h * 2; left; ) {
        unsigned n = left < CHUNK ? left : CHUNK;
        h9_lcd_write(buffer, n);
        left -= n;
    }
}

/* 3 x 5 font: digits, A-F and the letters used here. */
static unsigned glyph(char c)
{
    static const char chars[] = "0123456789ABCDEFGLSPTUVOR+";
    static const uint16_t bits[] = {
        075557, 026227, 071747, 071717, 055711, 074717, 074757, 071111, 075757, 075717,
        025755, 065656, 074447, 065556, 074647, 074644, 074557, 044447, 074717, 075744,
        072222, 055557, 055552, 075557, 065655, 002720,
    };
    for (unsigned i = 0; chars[i]; i++)
        if (chars[i] == c) return bits[i];
    return 0;
}

static void text(unsigned x, unsigned y, const char *s, uint16_t fg)
{
    for (; *s && x + 8 <= WIDTH; s++, x += 8) {
        uint8_t buffer[CHUNK];
        unsigned bits = glyph(*s), used = 0;
        h9_lcd_window(x, y, x + 7, y + 9);
        for (unsigned row = 0; row < 10; row++) {
            for (unsigned col = 0; col < 8; col++) {
                unsigned on = col < 6 && ((bits >> (14 - (row / 2) * 3 - col / 2)) & 1);
                uint16_t pixel = on ? fg : BLACK;
                buffer[used++] = (uint8_t)(pixel >> 8);
                buffer[used++] = (uint8_t)pixel;
                if (used == CHUNK) { h9_lcd_write(buffer, used); used = 0; }
            }
        }
        if (used) h9_lcd_write(buffer, used);
    }
}

static void hex(char *s, uint32_t v, unsigned digits)
{
    static const char d[] = "0123456789ABCDEF";
    for (unsigned i = 0; i < digits; i++)
        s[i] = d[(v >> (4 * (digits - 1 - i))) & 15];
    s[digits] = 0;
}

/* The registers of a page, in display order. */
static unsigned page_addresses(unsigned page, uint32_t *out)
{
    static const uint8_t dac[] = { 0x00, 0x04, 0x08, 0x10, 0x14, 0x28, 0x2C };
    unsigned n = 0;
    if (page == 0) {
        for (unsigned i = 0; i < sizeof dac; i++) out[n++] = 0x1E2F00 + dac[i];
    } else if (page == 1) {
        for (unsigned i = 0; i < 6; i++) out[n++] = 0x1E19C0 + 4 * i;
    } else {
        for (unsigned port = 0; port < 4; port++) {
            out[n++] = 0x1E5000 + 0x40 * port;
            out[n++] = 0x1E5000 + 0x40 * port + 8;
        }
    }
    return n;
}

static void read_all(uint32_t values[3][ROWS])
{
    for (unsigned page = 0; page < 3; page++) {
        uint32_t address[ROWS];
        unsigned n = page_addresses(page, address);
        for (unsigned i = 0; i < n; i++)
            values[page][i] = *(volatile uint32_t *)address[i];
    }
}

static void draw(unsigned page, int snapshot, uint32_t values[3][ROWS], uint32_t live[3][ROWS])
{
    static const char titles[] = "DAG";
    char title[12];
    const char *base = "AUDIO P D L";
    for (unsigned i = 0; i < 12; i++) title[i] = base[i];
    title[8] = titles[page];
    title[10] = snapshot ? 'S' : 'L';
    text(0, 0, title, GREEN);
    uint32_t address[ROWS];
    unsigned n = page_addresses(page, address);
    for (unsigned i = 0; i < ROWS; i++) {
        unsigned y = 12 + 12 * i;
        if (i >= n) { fill(0, y, WIDTH, 10, BLACK); continue; }
        char a[6], v[9];
        hex(a, address[i] & 0xFFFFF, 5);
        uint32_t value = snapshot ? values[page][i] : live[page][i];
        hex(v, value, 8);
        text(0, y, a + 1, CYAN);  /* low 4 hex digits of the address */
        text(40, y, v, snapshot && value != live[page][i] ? YELLOW : WHITE);
    }
    text(0, 116, "2PG 4SNAP 7TONE", WHITE);
}

void h9_main(void)
{
    uint32_t snap[3][ROWS], live[3][ROWS];
    unsigned page = 0, last = 0;
    int snapshot = 0, have = 0;
    uint32_t refresh = 0;
    fill(0, 0, WIDTH, HEIGHT, BLACK);
    read_all(live);
    read_all(snap);
    for (;;) {
        unsigned key = h9_key();
        if (key != last) {
            last = key;
            if (key == KEY_2) {
                page = (page + 1) % 3;
                fill(0, 12, WIDTH, 100, BLACK);
            } else if (key == KEY_4 && have) {
                snapshot = !snapshot;
            } else if (key == KEY_7) {
                h9_play_tone(0);
                h9_delay(60);
                read_all(snap);
                have = 1;
                snapshot = 1;
                h9_led(1);
            }
            refresh = 0;
        }
        if (refresh == 0) {
            read_all(live);
            draw(page, snapshot, snap, live);
            refresh = 25;
        }
        refresh--;
        h9_delay(10);
    }
}
