/* H9 C hello: the first C program for the TD-H9's main CPU, compiled with
 * JieLi's Pi32v2 clang. It measures the hardware, then draws a square you
 * move with Up / Down / Menu (left) / Back (right); the LED toggles on each
 * press.
 *
 * Measurements, in ticks of the stock timer-3 interrupt counter at RAM
 * 0x1D8B0 (incremented by the stock ISR at app+0x03126C):
 *   FILL  ticks to draw the full 160 x 128 gradient (display throughput)
 *   SQ    ticks for sixteen 16 x 16 squares (per-transfer overhead)
 *   LOOP  busy-loop iterations per tick (a CPU speed estimate)
 *   DLY   ticks taken by h9_delay(100) (the stock delay unit)
 *   TICK  the live counter: note it at two moments ~10 s apart on a
 *         stopwatch to get ticks per second, and so real times for the rest.
 * If the counter never moves, the screen says STUCK.
 *
 * Freestanding: no globals, no library calls (the build checks); constant
 * data (the font) is linked into the code cave. */
#include <stdint.h>

void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1);
void h9_lcd_write(const void *buffer, unsigned length);
unsigned h9_key(void);
void h9_delay(unsigned units);
void h9_led(unsigned on);

enum { WIDTH = 160, HEIGHT = 128, CHUNK = 128, SQUARE = 16, STEP = 8, PANEL = 46 };
enum { KEY_MENU = 12, KEY_UP = 13, KEY_DOWN = 14, KEY_BACK = 15 };
enum { WHITE = 0xffff, BLACK = 0x0000, YELLOW = 0xffe0, RED = 0xf800 };

#ifdef H9_HOST_TEST
uint32_t h9_host_ticks(void); /* host preview build (test/host_preview.c) */
static uint32_t ticks(void) { return h9_host_ticks(); }
#else
static uint32_t ticks(void) { return *(volatile uint32_t *)0x1d8b0; }
#endif

static uint16_t background(unsigned x, unsigned y)
{
    unsigned r = x * 31 / (WIDTH - 1);
    unsigned g = y * 63 / (HEIGHT - 1);
    return (uint16_t)((r << 11) | (g << 5) | (31 - r));
}

/* Draw a w x h rectangle at (x0, y0) through one window, in big-endian
 * RGB565: solid `color`, or the gradient when color == 1 (not a used color). */
static void draw(unsigned x0, unsigned y0, unsigned w, unsigned h, unsigned color)
{
    uint8_t buffer[CHUNK];
    unsigned used = 0;
    h9_lcd_window(x0, y0, x0 + w - 1, y0 + h - 1);
    for (unsigned y = y0; y < y0 + h; y++) {
        for (unsigned x = x0; x < x0 + w; x++) {
            uint16_t pixel = color == 1 ? background(x, y) : (uint16_t)color;
            buffer[used++] = (uint8_t)(pixel >> 8);
            buffer[used++] = (uint8_t)pixel;
            if (used == CHUNK) {
                h9_lcd_write(buffer, used);
                used = 0;
            }
        }
    }
    if (used) h9_lcd_write(buffer, used);
}

/* 3 x 5 font, rows top to bottom, 3 bits each (MSB = left): the digits and
 * the letters the labels use. Anything else is blank. */
static unsigned glyph(char c)
{
    static const char chars[] = "0123456789CDFIKLOPQSTUY";
    static const uint16_t bits[] = {
        075557, 026227, 071747, 071717, 055711, 074717, 074757, 071111, 075757, 075717,
        074447, 065556, 074644, 072227, 055655, 044447, 075557, 075744, 075573, 074717,
        072222, 055557, 055222,
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
    for (; label[i] && i < 5; i++) s[i] = label[i];
    for (; i < 12; i++) s[i] = ' ';
    s[12] = 0;
    for (i = 11; ; i--) {
        s[i] = (char)('0' + value % 10);
        value /= 10;
        if (!value || i == 5) break;
    }
    text(0, y, s, color, BLACK);
}

void h9_main(void)
{
    uint32_t start = ticks();
    draw(0, 0, WIDTH, HEIGHT, 1);
    uint32_t fill = ticks() - start;

    start = ticks();
    for (unsigned i = 0; i < 16; i++) draw(72, 64, SQUARE, SQUARE, 1); /* redraws, no visible change */
    uint32_t squares = ticks() - start;

    /* Busy loop until the counter advances 25 ticks (capped, in case it never moves). */
    uint32_t loops = 0, first = ticks(), now = first;
    while (now - first < 25 && loops < 200000000u) {
        loops++;
        now = ticks();
    }
    unsigned stuck = now == first;

    start = ticks();
    h9_delay(100);
    uint32_t delay = ticks() - start;

    draw(0, 0, WIDTH, PANEL, BLACK);
    if (stuck) {
        text(0, 0, "TICK STUCK", RED, BLACK);
    } else {
        line(0, "FILL", fill, WHITE);
        line(12, "SQ", squares, WHITE);
        line(24, "LOOP", loops / 25, WHITE);
        line(36, "DLY", delay, WHITE);
    }

    unsigned x = (WIDTH - SQUARE) / 2, y = 72, held = 0, led = 1;
    uint32_t shown = ~0u;
    draw(x, y, SQUARE, SQUARE, WHITE);
    h9_led(led);
    for (;;) {
        uint32_t t = ticks();
        if (!stuck && t / 4 != shown) {        /* refresh TICK at most every 4 ticks */
            shown = t / 4;
            line(HEIGHT - 10, "TICK", t, YELLOW);
        }
        unsigned key = h9_key();
        if (!key) {
            held = 0;
        } else if (!held) {
            unsigned nx = x, ny = y;
            held = 1;
            if (key == KEY_UP && y >= PANEL + STEP) ny = y - STEP;
            if (key == KEY_DOWN && y + SQUARE + STEP <= HEIGHT - 12) ny = y + STEP;
            if (key == KEY_MENU && x >= STEP) nx = x - STEP;
            if (key == KEY_BACK && x + SQUARE + STEP <= WIDTH) nx = x + STEP;
            if (nx != x || ny != y) {
                draw(x, y, SQUARE, SQUARE, 1);
                x = nx;
                y = ny;
                draw(x, y, SQUARE, SQUARE, WHITE);
            }
            led = !led;
            h9_led(led);
        }
        h9_delay(10);
    }
}
