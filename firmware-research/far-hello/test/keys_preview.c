/* Host check of keys.c: stand-in hardware, scripted key byte (nothing, then
 * the 8 key's expected code 5 held), screen saved when the program calls the
 * USB loader; fails if it never does or leaves the screen bounds.
 *   cc -std=c99 -Wall -Werror -DH9_HOST_TEST -o build/keys-preview test/keys_preview.c keys.c
 *   build/keys-preview build/keys-preview.ppm */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void h9_main(void);
static uint16_t screen[128][160];
static unsigned wx0, wy0, wx1, wy1, cx, cy, half, scans;
static uint8_t hi;
static uint32_t now = 1000;
static const char *out;

uint32_t h9_host_ticks(void) { return now; }
unsigned h9_host_byte(unsigned address) { return address == 0xc39d ? 14 : 0; }
void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1)
{
    if (x1 >= 160 || y1 >= 128 || x0 > x1 || y0 > y1) { fprintf(stderr, "bad window\n"); exit(1); }
    wx0 = cx = x0; wy0 = cy = y0; wx1 = x1; wy1 = y1; half = 0;
}
void h9_lcd_write(const void *buffer, unsigned length)
{
    const uint8_t *b = buffer;
    if (length > 128) { fprintf(stderr, "write too long\n"); exit(1); }
    for (unsigned i = 0; i < length; i++) {
        if (!half) { hi = b[i]; half = 1; continue; }
        half = 0;
        if (cy > wy1) { fprintf(stderr, "write past window\n"); exit(1); }
        screen[cy][cx] = (uint16_t)(hi << 8 | b[i]);
        if (++cx > wx1) { cx = wx0; cy++; }
    }
}
unsigned h9_key(void)
{
    if (++scans > 5000) { fprintf(stderr, "never entered the USB loader\n"); exit(1); }
    return scans < 50 ? 0 : 5;   /* idle, then the 8 key held */
}
void h9_delay(unsigned units) { now += units / 10 ? units / 10 : 1; }
void h9_led(unsigned on) { (void)on; }
void h9_enter_usb_loader(void)
{
    FILE *f = fopen(out, "wb");
    fprintf(f, "P6\n160 128\n255\n");
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 160; x++) {
            uint16_t p = screen[y][x];
            fputc((p >> 11) * 255 / 31, f); fputc(((p >> 5) & 63) * 255 / 63, f); fputc((p & 31) * 255 / 31, f);
        }
    fclose(f);
    printf("entered USB loader after %u key scans (%u ticks held)\n", scans, now - 1000);
    exit(0);
}
int main(int argc, char **argv) { out = argc > 1 ? argv[1] : "keys.ppm"; h9_main(); return 1; }
