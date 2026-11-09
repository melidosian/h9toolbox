/* H9 C hello: the first C program for the TD-H9's main CPU, compiled with
 * JieLi's Pi32v2 clang. Draws a full-screen gradient, then a white square
 * you move with Up / Down / Menu (left) / Back (right); the LED toggles on
 * each press. Freestanding: no globals, no constant data, no library calls
 * (the build checks for undefined symbols). */
#include <stdint.h>

void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1);
void h9_lcd_write(const void *buffer, unsigned length);
unsigned h9_key(void);
void h9_delay(unsigned units);
void h9_led(unsigned on);

enum { WIDTH = 160, HEIGHT = 128, CHUNK = 128, SQUARE = 16, STEP = 8 };
enum { KEY_MENU = 12, KEY_UP = 13, KEY_DOWN = 14, KEY_BACK = 15 };

static uint16_t background(unsigned x, unsigned y)
{
    unsigned r = x * 31 / (WIDTH - 1);
    unsigned g = y * 63 / (HEIGHT - 1);
    return (uint16_t)((r << 11) | (g << 5) | (31 - r));
}

/* Draw a rectangle pixel by pixel, in big-endian RGB565, through one window.
 * square != 0 paints solid white; otherwise it repaints the background. */
static void draw(unsigned x0, unsigned y0, unsigned w, unsigned h, int square)
{
    uint8_t buffer[CHUNK];
    unsigned used = 0;
    h9_lcd_window(x0, y0, x0 + w - 1, y0 + h - 1);
    for (unsigned y = y0; y < y0 + h; y++) {
        for (unsigned x = x0; x < x0 + w; x++) {
            uint16_t pixel = square ? 0xffff : background(x, y);
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

void h9_main(void)
{
    unsigned x = (WIDTH - SQUARE) / 2, y = (HEIGHT - SQUARE) / 2;
    unsigned held = 0, led = 1;
    draw(0, 0, WIDTH, HEIGHT, 0);
    draw(x, y, SQUARE, SQUARE, 1);
    h9_led(led);
    for (;;) {
        unsigned key = h9_key();
        if (!key) {
            held = 0;
        } else if (!held) {
            unsigned nx = x, ny = y;
            held = 1;
            if (key == KEY_UP && y >= STEP) ny = y - STEP;
            if (key == KEY_DOWN && y + SQUARE + STEP <= HEIGHT) ny = y + STEP;
            if (key == KEY_MENU && x >= STEP) nx = x - STEP;
            if (key == KEY_BACK && x + SQUARE + STEP <= WIDTH) nx = x + STEP;
            if (nx != x || ny != y) {
                draw(x, y, SQUARE, SQUARE, 0);
                x = nx;
                y = ny;
                draw(x, y, SQUARE, SQUARE, 1);
            }
            led = !led;
            h9_led(led);
        }
        h9_delay(10);
    }
}
