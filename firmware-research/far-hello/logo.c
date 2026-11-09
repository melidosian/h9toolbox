/* Far logo: the first program that reads the 16 MiB resource flash from C.
 * Runs from main flash 0xEA000 (started by holding Back on H9 Toolbox
 * Firmware with the far launcher). It streams the boot logo from the
 * resource partition (selector 5, offset 0: 160 x 128 big-endian RGB565, the
 * LCD's own format) straight to the screen, then times plain reads.
 *
 * Readouts, in 10 ms ticks of the stock timer counter at RAM 0x1D8B0:
 *   SHOW   read + draw the whole logo, 128 bytes at a time
 *   RD128  read the logo's 40,960 bytes in 128-byte calls (no drawing)
 *   RD512  the same in 512-byte calls
 *   SUM    16-bit byte sum of the logo; both reads must agree, else BAD
 * Up redraws the logo, Down brings the readouts back. The LED toggles on
 * each press. Bluetooth keeps running underneath (stock tasks are untouched).
 *
 * Freestanding: no globals, no library calls (the build checks). */
#include <stdint.h>

void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1);
void h9_lcd_write(const void *buffer, unsigned length);
unsigned h9_key(void);
void h9_delay(unsigned units);
void h9_led(unsigned on);
int h9_res_read(unsigned partition, uint32_t offset, void *buffer, unsigned length);

enum { WIDTH = 160, HEIGHT = 128, CHUNK = 128, BIG = 512, PANEL = 50 };
enum { RESOURCE = 5, LOGO_BYTES = WIDTH * HEIGHT * 2 };
enum { KEY_UP = 13, KEY_DOWN = 14 };
enum { WHITE = 0xffff, BLACK = 0x0000, YELLOW = 0xffe0, RED = 0xf800, GREEN = 0x07e0 };

#ifdef H9_HOST_TEST
uint32_t h9_host_ticks(void);
static uint32_t ticks(void) { return h9_host_ticks(); }
#else
static uint32_t ticks(void) { return *(volatile uint32_t *)0x1d8b0; }
#endif

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

/* Stream the logo from the resource flash to the LCD; returns its byte sum. */
static uint32_t show_logo(void)
{
    uint8_t buffer[CHUNK];
    uint32_t sum = 0;
    h9_lcd_window(0, 0, WIDTH - 1, HEIGHT - 1);
    for (uint32_t offset = 0; offset < LOGO_BYTES; offset += CHUNK) {
        h9_res_read(RESOURCE, offset, buffer, CHUNK);
        for (unsigned i = 0; i < CHUNK; i++) sum += buffer[i];
        h9_lcd_write(buffer, CHUNK);
    }
    return sum;
}

/* Read the logo in `size`-byte calls into `buffer`; returns its byte sum. */
static uint32_t read_logo(uint8_t *buffer, unsigned size)
{
    uint32_t sum = 0;
    for (uint32_t offset = 0; offset < LOGO_BYTES; offset += size) {
        h9_res_read(RESOURCE, offset, buffer, size);
        for (unsigned i = 0; i < size; i++) sum += buffer[i];
    }
    return sum;
}

/* 3 x 5 font, rows top to bottom, 3 bits each (MSB = left). */
static unsigned glyph(char c)
{
    static const char chars[] = "0123456789ABDEHKLMOPRSUW";
    static const uint16_t bits[] = {
        075557, 026227, 071747, 071717, 055711, 074717, 074757, 071111, 075757, 075717,
        025755, 065656, 065556, 074647, 055755, 055655, 044447, 057755, 075557, 075744,
        065655, 074717, 055557, 055775,
    };
    for (unsigned i = 0; chars[i]; i++)
        if (chars[i] == c) return bits[i];
    return 0;
}

/* Text at 2x scale: 6 x 10 pixels per character, 8 px advance. */
static void text(unsigned x, unsigned y, const char *s, uint16_t fg, uint16_t bg)
{
    for (; *s; s++, x += 8) {
        uint8_t buffer[CHUNK];
        unsigned bits = glyph(*s), used = 0;
        h9_lcd_window(x, y, x + 7, y + 9);
        for (unsigned row = 0; row < 10; row++) {
            for (unsigned col = 0; col < 8; col++) {
                unsigned on = col < 6 && ((bits >> (14 - (row / 2) * 3 - col / 2)) & 1);
                uint16_t pixel = on ? fg : bg;
                buffer[used++] = (uint8_t)(pixel >> 8);
                buffer[used++] = (uint8_t)pixel;
                if (used == CHUNK) { h9_lcd_write(buffer, used); used = 0; }
            }
        }
        if (used) h9_lcd_write(buffer, used);
    }
}

/* "LABEL" followed by a right-aligned unsigned number, 12 characters. */
static void line(unsigned y, const char *label, uint32_t value, uint16_t color)
{
    char s[13];
    unsigned i = 0;
    for (; label[i] && i < 6; i++) s[i] = label[i];
    for (; i < 12; i++) s[i] = ' ';
    s[12] = 0;
    for (i = 11; ; i--) {
        s[i] = (char)('0' + value % 10);
        value /= 10;
        if (!value || i == 6) break;
    }
    text(0, y, s, color, BLACK);
}

void h9_main(void)
{
    uint8_t big[BIG];
    uint32_t start = ticks();
    uint32_t sum = show_logo();
    uint32_t shown = ticks() - start;
    h9_delay(3000); /* let the logo be seen */

    start = ticks();
    uint32_t sum128 = read_logo(big, CHUNK);
    uint32_t read128 = ticks() - start;
    start = ticks();
    uint32_t sum512 = read_logo(big, BIG);
    uint32_t read512 = ticks() - start;
    unsigned good = sum128 == sum && sum512 == sum;

    unsigned panel = 1, held = 0, led = 1;
    h9_led(led);
    for (;;) {
        if (panel) {
            fill(0, 0, WIDTH, PANEL, BLACK);
            line(0, "SHOW", shown, WHITE);
            line(12, "RD128", read128, WHITE);
            line(24, "RD512", read512, WHITE);
            line(36, good ? "SUM" : "BAD", sum & 0xffff, good ? GREEN : RED);
            panel = 0;
        }
        unsigned key = h9_key();
        if (!key) {
            held = 0;
        } else if (!held) {
            held = 1;
            if (key == KEY_UP) show_logo();
            if (key == KEY_DOWN) panel = 1;
            led = !led;
            h9_led(led);
        }
        h9_delay(10);
    }
}
