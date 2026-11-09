/* Host preview of logo.c: compile it natively with stand-in hardware calls
 * that draw into a 160 x 128 framebuffer and read the "resource flash" from
 * a raw 40,960-byte big-endian RGB565 logo file, then save the screen as PPM.
 * The stand-in timer advances one tick per read; keys are scripted.
 *   cc -std=c99 -Wall -Werror -DH9_HOST_TEST -o build/logo-preview test/logo_preview.c logo.c
 *   build/logo-preview <logo.bin> build/logo-preview.ppm */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void h9_main(void);
static uint16_t screen[128][160];
static uint8_t flash[40960];
static unsigned wx0, wy0, wx1, wy1, cx, cy, half, calls;
static uint8_t hi;
static const char *out;

uint32_t h9_host_ticks(void) { static uint32_t t = 1000; return t++; }
void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1)
{
    if (x1 >= 160 || y1 >= 128 || x0 > x1 || y0 > y1) { fprintf(stderr, "bad window %u,%u-%u,%u\n", x0, y0, x1, y1); exit(1); }
    wx0 = cx = x0; wy0 = cy = y0; wx1 = x1; wy1 = y1; half = 0;
}
void h9_lcd_write(const void *buffer, unsigned length)
{
    const uint8_t *b = buffer;
    if (length > 128) { fprintf(stderr, "write too long: %u\n", length); exit(1); }
    for (unsigned i = 0; i < length; i++) {
        if (!half) { hi = b[i]; half = 1; continue; }
        half = 0;
        if (cy > wy1) { fprintf(stderr, "write past window\n"); exit(1); }
        screen[cy][cx] = (uint16_t)(hi << 8 | b[i]);
        if (++cx > wx1) { cx = wx0; cy++; }
    }
}
int h9_res_read(unsigned partition, uint32_t offset, void *buffer, unsigned length)
{
    if (partition != 5 || offset + length > sizeof flash) { fprintf(stderr, "bad read %u %u %u\n", partition, offset, length); exit(1); }
    memcpy(buffer, flash + offset, length);
    return 0;
}
static void save(void)
{
    FILE *f = fopen(out, "wb");
    fprintf(f, "P6\n160 128\n255\n");
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 160; x++) {
            uint16_t p = screen[y][x];
            fputc((p >> 11) * 255 / 31, f); fputc(((p >> 5) & 63) * 255 / 63, f); fputc((p & 31) * 255 / 31, f);
        }
    fclose(f);
}
unsigned h9_key(void)
{
    /* Script: idle twice, then stop and save the readout screen. */
    if (calls++ < 2) return 0;
    save();
    exit(0);
}
void h9_delay(unsigned units) { (void)units; }
void h9_led(unsigned on) { (void)on; }

int main(int argc, char **argv)
{
    FILE *f = argc > 1 ? fopen(argv[1], "rb") : NULL;
    if (!f || fread(flash, 1, sizeof flash, f) != sizeof flash) { fprintf(stderr, "need a 40,960-byte logo file\n"); return 1; }
    fclose(f);
    out = argc > 2 ? argv[2] : "logo-preview.ppm";
    h9_main();
    return 1;
}
