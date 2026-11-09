/* First JieLi-clang test: fill a 160 x 128 RGB565 big-endian frame (the
 * boot-logo format) with a gradient. No stock calls, no globals: it only
 * checks that the compiler emits Pi32v2 r3 code our tools can read. */
#include <stdint.h>

#define WIDTH 160
#define HEIGHT 128

void h9_gradient(uint8_t *frame)
{
    for (unsigned y = 0; y < HEIGHT; y++) {
        for (unsigned x = 0; x < WIDTH; x++) {
            unsigned r = x * 31 / (WIDTH - 1);
            unsigned g = y * 63 / (HEIGHT - 1);
            unsigned b = 31 - r;
            uint16_t pixel = (uint16_t)((r << 11) | (g << 5) | b);
            frame[(y * WIDTH + x) * 2] = (uint8_t)(pixel >> 8);
            frame[(y * WIDTH + x) * 2 + 1] = (uint8_t)pixel;
        }
    }
}
