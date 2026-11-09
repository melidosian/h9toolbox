// SPDX-License-Identifier: GPL-2.0-or-later
// Host test of the TD-H9 Doom firmware: the exact firmware source set
// (build.py sources()) compiled natively with -DTD_HOST, with this file in
// place of the stock helpers and the RAM block:
//   - LCD: a 160 x 128 framebuffer (window semantics checked), saved as PPM
//   - resource flash: a 16 MiB chip image (directory + pack) read from a file
//   - keys: a script (idle, then Up held); time: one tick per read
//   - RAM: __ram_end / __frame_start / dmc_tic set on host arrays, as the
//     firmware link does on the RAM block
// Usage: td_host <chip image> <out dir> [tics]
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "td.h"

void td_main(void);

// RAM symbols of the firmware link (build.py script()).
uint8_t td_host_frame[15208 + 64] __attribute__((aligned(16)));
uint8_t td_host_arena[10680] __attribute__((aligned(16)));
__asm__(".globl __frame_start\n.set __frame_start, td_host_frame\n"
        ".globl __frame_max\n.set __frame_max, 15208\n"
        ".globl dmc_tic\n.set dmc_tic, td_host_frame + 6912\n"
        ".globl __ram_end\n.set __ram_end, td_host_arena + 10680\n");

static uint16_t fb[TD_H][TD_W];
static unsigned wx0, wy0, wx1, wy1, cx, cy, half;
static uint8_t hi;
static uint8_t *chip;
static const char *outdir;
static uint32_t now = 1000, keyscans, max_tics = 140, frames_saved;

uint32_t td_ticks(void) { return now++; }

void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1)
{
    if (x1 >= TD_W || y1 >= TD_H || x0 > x1 || y0 > y1) { fprintf(stderr, "bad window %u,%u-%u,%u\n", x0, y0, x1, y1); exit(3); }
    wx0 = cx = x0; wy0 = cy = y0; wx1 = x1; wy1 = y1; half = 0;
}

void h9_lcd_write(const void *buffer, unsigned length)
{
    const uint8_t *b = buffer;
    if (length > 128) { fprintf(stderr, "write of %u bytes\n", length); exit(3); }
    for (unsigned i = 0; i < length; i++) {
        if (!half) { hi = b[i]; half = 1; continue; }
        half = 0;
        if (cy > wy1) { fprintf(stderr, "write past window\n"); exit(3); }
        fb[cy][cx] = (uint16_t)(hi << 8 | b[i]);
        if (++cx > wx1) { cx = wx0; cy++; }
    }
}

static void save(const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", outdir, name);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", TD_W, TD_H);
    for (int y = 0; y < TD_H; y++)
        for (int x = 0; x < TD_W; x++) {
            uint16_t p = fb[y][x];
            fputc((p >> 11) * 255 / 31, f); fputc(((p >> 5) & 63) * 255 / 63, f); fputc((p & 31) * 255 / 31, f);
        }
    fclose(f);
    frames_saved++;
}

// I_StartTic calls this once per tic; the power-on recovery check calls it first.
unsigned h9_key(void)
{
    keyscans++;
    if (keyscans == 31) save("status.ppm");
    if (keyscans == 31 + 35) save("tic035.ppm");
    if (keyscans == 31 + 70) save("tic070.ppm");
    if (keyscans == 31 + max_tics) {
        save("final.ppm");
        printf("ran %u tics, %u frames saved\n", max_tics, frames_saved);
        exit(0);
    }
    return keyscans > 31 + 40 ? TD_KEY_2 : 0;  // walk forward (key 2) after 40 tics
}

void h9_delay(unsigned ms) { now += ms / 10 ? ms / 10 : 1; }
void h9_led(unsigned on) { (void)on; }
void h9_spi2_setup(void) {}
void h9_enter_usb_loader(void) { puts("USB loader"); exit(4); }
void h9_post_core_event(unsigned code, unsigned value) { (void)code; (void)value; }
void h9_irq_off(void) {}
void h9_irq_on(void) {}
void h9_speaker_gate(unsigned source, unsigned on) { (void)source; (void)on; }
void h9_dac_rate(void *dac, unsigned rate) { (void)dac; (void)rate; }
void h9_dac_start(void *dac) { (void)dac; }
void h9_dac_write128(const void *pcm) { (void)pcm; }
void h9_audio_volume(unsigned state, unsigned level) { (void)state; (void)level; }
int h9_res_read(unsigned p, uint32_t o, void *b, unsigned n) { (void)p; (void)o; (void)b; (void)n; exit(5); }

void h9_nor_read_raw(void *buffer, uint32_t addr, uint32_t length)
{
    if (addr + length > 0x1000000u) { fprintf(stderr, "read past chip\n"); exit(3); }
    memcpy(buffer, chip + addr, length);
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: td_host <chip image> <out dir> [tics]\n"); return 2; }
    chip = malloc(0x1000000);
    FILE *f = fopen(argv[1], "rb");
    if (!f || fread(chip, 1, 0x1000000, f) != 0x1000000) { fprintf(stderr, "need a 16 MiB chip image\n"); return 2; }
    fclose(f);
    outdir = argv[2];
    if (argc > 3) max_tics = (uint32_t)atoi(argv[3]);
    td_main();
    return 1;
}
