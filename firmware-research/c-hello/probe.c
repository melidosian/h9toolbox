/* Far-region probe: runs from the app cave at my_app entry (where C-hello
 * ran), which is known to execute at boot. Shows, refreshed every second,
 * what the CPU reads through the flash mapping at the start of the far
 * region (flash 0xEA000 = 0x01EE5000) and, for comparison, at app flash.
 * Answers why far code does not start at boot (runs 1-3, 2026-11-09):
 * plain marker 48395831 = readable as stored; other values = decrypted or
 * unmapped; a change over time = when stock switches the mapping.
 *
 * Freestanding: no globals, no library calls (the build checks). */
#include <stdint.h>

void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1);
void h9_lcd_write(const void *buffer, unsigned length);
unsigned h9_key(void);
void h9_delay(unsigned units);
void h9_led(unsigned on);

enum { WIDTH = 160, HEIGHT = 128, CHUNK = 128 };
enum { WHITE = 0xffff, BLACK = 0x0000, YELLOW = 0xffe0 };

#ifdef H9_HOST_TEST
uint32_t h9_host_ticks(void);
uint32_t h9_host_word(uint32_t address);
static uint32_t ticks(void) { return h9_host_ticks(); }
static uint32_t word(uint32_t address) { return h9_host_word(address); }
#else
static uint32_t ticks(void) { return *(volatile uint32_t *)0x1d8b0; }
static uint32_t word(uint32_t address) { return *(volatile uint32_t *)address; }
#endif

/* 3 x 5 font: hex digits and the label letters. */
static unsigned glyph(char c)
{
    static const char chars[] = "0123456789ABCDEFPTU";
    static const uint16_t bits[] = {
        075557, 026227, 071747, 071717, 055711, 074717, 074757, 071111, 075757, 075717,
        025755, 065656, 074447, 065556, 074647, 074644, 075744, 072222, 055557,
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

/* Two-letter label then 8 hex digits. */
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
    uint32_t start = ticks(), shown = ~0u;
    for (;;) {
        uint32_t seconds = (ticks() - start) / 100;
        if (seconds != shown) {
            shown = seconds;
            hexline(0, "F0", word(0x01ee5000), YELLOW);   /* far +0: marker */
            hexline(12, "F1", word(0x01ee5004), YELLOW);  /* far +4: 0 */
            hexline(24, "F2", word(0x01ee5008), YELLOW);  /* far +8: first code */
            hexline(40, "A0", word(0x01e88280), WHITE);   /* app: my_app entry (patched call) */
            hexline(52, "A1", word(0x01e00100), WHITE);   /* app: first app word */
            hexline(68, "UP", seconds, WHITE);
            h9_led(seconds & 1);
        }
        h9_delay(10);
    }
}
