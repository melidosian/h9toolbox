/* Probe 2: can the CPU *execute* far-region code at boot, and in which form?
 * Runs from the app cave at my_app entry. The far blob (probe2_far.bin) holds
 * two tiny functions with no data access:
 *   0x01EE5100 (flash 0xEA100) stored plain:     returns 0x11111111
 *   0x01EE5200 (flash 0xEA200) stored encrypted: returns 0x22222222
 * Up calls the plain one, Down the encrypted one (one per boot: the wrong
 * form executes garbage). The result line shows what came back; if the
 * screen freezes at "CALL", that form does not execute.
 *
 * Freestanding: no globals, no library calls (the build checks). */
#include <stdint.h>

void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1);
void h9_lcd_write(const void *buffer, unsigned length);
unsigned h9_key(void);
void h9_delay(unsigned units);
void h9_led(unsigned on);

enum { CHUNK = 128, KEY_UP = 13, KEY_DOWN = 14 };
enum { WHITE = 0xffff, BLACK = 0x0000, YELLOW = 0xffe0, GREEN = 0x07e0 };
typedef uint32_t (*far_fn)(void);

static uint32_t ticks(void) { return *(volatile uint32_t *)0x1d8b0; }

static unsigned glyph(char c)
{
    static const char chars[] = "0123456789ABCDEFLNPRSTUW";
    static const uint16_t bits[] = {
        075557, 026227, 071747, 071717, 055711, 074717, 074757, 071111, 075757, 075717,
        025755, 065656, 074447, 065556, 074647, 074644, 044447, 065555, 075744, 065655,
        074717, 072222, 055557, 055775,
    };
    for (unsigned i = 0; chars[i]; i++)
        if (chars[i] == c) return bits[i];
    return 0;
}

static void text(unsigned x, unsigned y, const char *s, uint16_t fg)
{
    for (; *s; s++, x += 8) {
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

static void hexline(unsigned y, const char *label, uint32_t value, uint16_t color)
{
    static const char digits[] = "0123456789ABCDEF";
    char s[12];
    s[0] = label[0]; s[1] = label[1]; s[2] = ' ';
    for (unsigned i = 0; i < 8; i++) s[3 + i] = digits[(value >> (28 - 4 * i)) & 15];
    s[11] = 0;
    text(0, y, s, color);
}

void h9_main(void)
{
    h9_led(1);
    text(0, 0, "UP PLAIN", WHITE);
    text(0, 12, "DN ENC", WHITE);
    uint32_t start = ticks(), shown = ~0u;
    unsigned done = 0;
    for (;;) {
        uint32_t seconds = (ticks() - start) / 100;
        if (seconds != shown) {
            shown = seconds;
            hexline(100, "UP", seconds, WHITE);
            h9_led(seconds & 1);
        }
        unsigned key = h9_key();
        if (!done && (key == KEY_UP || key == KEY_DOWN)) {
            done = 1;
            far_fn fn = (far_fn)(key == KEY_UP ? 0x01ee5100u : 0x01ee5200u);
            text(0, 36, key == KEY_UP ? "CALL PLAIN" : "CALL ENC", YELLOW);
            uint32_t result = fn();
            hexline(52, "RT", result, GREEN);
        }
        h9_delay(10);
    }
}
