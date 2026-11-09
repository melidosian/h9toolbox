// SPDX-License-Identifier: GPL-2.0-or-later
// TD-H9 port of DMC100-Doom: the program entry, called by the startup in the
// app cave (td_entry.s) once the panel is up. Lives in the far region.
//
// 1. .data copied from flash, .bss cleared (the engine's fixed core lives in
//    the RAM block taken from the end of the stock heap).
// 2. Recovery: the 3 key held at power-on enters the mask-ROM USB loader
//    (ROM 0x02000D68), the same mode hold-8 gives on stock.
// 3. A status screen: heap use (stock counters at RAM 0x1D8A8/0x1D8AA) and
//    the pack found by the upload directory.
// 4. img_init (the data layer's header and section checks), then
//    D_DoomMain with -warp 1 1.
// A failure stays on screen; hold 3 for 2 s then enters the USB loader.

#include <stdint.h>
#include <string.h>

#include "doomtype.h"
#include "d_main.h"
#include "img.h"
#include "m_argv.h"

#include "td.h"

extern uint8_t __data_load[], __data_start[], __data_end[], __bss_start[], __bss_end[];

enum { BLACK = 0x0000, WHITE = 0xffff, YELLOW = 0xffe0, RED = 0xf800, GREEN = 0x07e0 };

void td_fill(unsigned x0, unsigned y0, unsigned w, unsigned h, uint16_t color)
{
    uint8_t buffer[128];
    for (unsigned i = 0; i < sizeof buffer; i += 2) {
        buffer[i] = (uint8_t)(color >> 8);
        buffer[i + 1] = (uint8_t)color;
    }
    h9_lcd_window(x0, y0, x0 + w - 1, y0 + h - 1);
    for (unsigned left = w * h * 2; left; ) {
        unsigned n = left < sizeof buffer ? left : sizeof buffer;
        h9_lcd_write(buffer, n);
        left -= n;
    }
}

// 3 x 5 font at 2x: digits, upper-case letters and a few marks.
static unsigned glyph(char c)
{
    static const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-:/.";
    static const uint16_t bits[] = {
        075557, 026227, 071747, 071717, 055711, 074717, 074757, 071111, 075757, 075717,
        025755, 065656, 074447, 065556, 074647, 074644, 074557, 055755, 072227, 011157,
        055655, 044447, 057755, 065555, 075557, 075744, 075573, 065655, 074717, 072222,
        055557, 055552, 055775, 055255, 055222, 071247, 000700, 002020, 011244, 000002,
    };
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);
    for (unsigned i = 0; chars[i]; i++)
        if (chars[i] == c)
            return bits[i];
    return 0;
}

void td_text(unsigned x, unsigned y, const char *s, uint16_t fg, uint16_t bg)
{
    for (; *s && x + 8 <= TD_W; s++, x += 8) {
        uint8_t buffer[128];
        unsigned bits = glyph(*s), used = 0;
        h9_lcd_window(x, y, x + 7, y + 9);
        for (unsigned row = 0; row < 10; row++) {
            for (unsigned col = 0; col < 8; col++) {
                unsigned on = col < 6 && ((bits >> (14 - (row / 2) * 3 - col / 2)) & 1);
                uint16_t pixel = on ? fg : bg;
                buffer[used++] = (uint8_t)(pixel >> 8);
                buffer[used++] = (uint8_t)pixel;
                if (used == sizeof buffer) { h9_lcd_write(buffer, used); used = 0; }
            }
        }
        if (used)
            h9_lcd_write(buffer, used);
    }
}

static void number(unsigned x, unsigned y, uint32_t v, uint16_t color)
{
    char s[11];
    int i = 10;
    s[i] = 0;
    do {
        s[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v && i);
    td_text(x, y, s + i, color, BLACK);
}

static void usb_loader(void)
{
    td_fill(0, 0, TD_W, TD_H, BLACK);
    td_text(56, 56, "USB", GREEN, BLACK);
    h9_delay(1000);
    h9_enter_usb_loader();
}

// Stays here; holding 3 for 2 s enters the USB loader.
void td_fatal(const char *what, const char *detail)
{
    td_fill(0, 0, TD_W, TD_H, BLACK);
    td_text(0, 0, what, RED, BLACK);
    td_text(0, 14, detail, WHITE, BLACK);
    td_text(0, 108, "HOLD 3: USB", YELLOW, BLACK);
    uint32_t since = 0;
    int held = 0;
    for (;;) {
        if (h9_key() == TD_KEY_3) {
            if (!held) { held = 1; since = td_ticks(); }
            if (td_ticks() - since >= 200)
                usb_loader();
        } else {
            held = 0;
        }
        h9_delay(10);
    }
}

// Key test: the key byte of whatever is pressed, and the last eight codes
// seen. Stays until power-off (hold 3 for 2 s: USB loader).
static void key_test(void)
{
    td_fill(0, 0, TD_W, TD_H, BLACK);
    td_text(0, 0, "KEY TEST", YELLOW, BLACK);
    td_text(0, 108, "HOLD 3: USB", WHITE, BLACK);
    while (h9_key() == TD_KEY_MENU)
        h9_delay(10);
    unsigned last = ~0u, seen[8] = { 0 }, n = 0;
    uint32_t since = 0;
    for (;;) {
        unsigned key = h9_key();
        if (key != last) {
            last = key;
            td_fill(0, 24, TD_W, 12, BLACK);
            td_text(0, 24, "NOW", WHITE, BLACK);
            number(40, 24, key, GREEN);
            if (key) {
                seen[n++ % 8] = key;
                td_fill(0, 44, TD_W, 24, BLACK);
                for (unsigned i = 0; i < 8 && i < n; i++)
                    number(40 * (i % 4), 44 + 12 * (i / 4), seen[(n - 1 - i) % 8], i ? WHITE : YELLOW);
            }
            since = td_ticks();
        }
        if (key == TD_KEY_3 && td_ticks() - since >= 200)
            usb_loader();
        h9_delay(10);
    }
}

// Bluetooth upload mode (hold 2 at power-on, or no game data). Doom does not
// start: its RAM block (0x22000-0x2BF00, taken from the end of the stock heap
// at boot) goes back to the heap, so the Bluetooth stack has stock's heap,
// then app_core is asked for mode 3 (stock's running mode, which starts the
// Bluetooth stack; build 11 showed the switch works but ran out of heap
// beside Doom). From here on nothing in the RAM block (.data/.bss) is used.
//
// Stock heap (app+0x5FA9E): 128-byte blocks, a free-bit per block at
// 0x1D7D0 (word block/32), a summary bit per word at 0x1D7D0+192, the block
// total at 0x1D8AA and the used count at 0x1D8A8.
#define HEAP_BITS ((volatile uint32_t *)0x1D7D0)
#define HEAP_SUMMARY ((volatile uint32_t *)(0x1D7D0 + 192))
#define HEAP_USED (*(volatile uint16_t *)0x1D8A8)
#define HEAP_TOTAL (*(volatile uint16_t *)0x1D8AA)
#define CORE ((volatile uint8_t *)0x11AD0)

static void heap_give_back(void)
{
    h9_irq_off();
    for (uint32_t block = 0x22000 >> 7; block < (0x2BF00 >> 7); block++) {
        HEAP_BITS[block >> 5] |= 1u << (block & 31);
        HEAP_SUMMARY[block >> 10] |= 1u << ((block >> 5) & 31);
    }
    HEAP_TOTAL = (uint16_t)(HEAP_TOTAL + ((0x2BF00 - 0x22000) >> 7));
    h9_irq_on();
}

static void upload_mode(const char *why)
{
    td_fill(0, 0, TD_W, TD_H, BLACK);
    td_text(0, 0, "H9 DOOM", YELLOW, BLACK);
    td_text(0, 14, "BLUETOOTH UPLOAD", GREEN, BLACK);
    td_text(0, 28, why, WHITE, BLACK);
    heap_give_back();
    CORE[100] = 3;                  // request mode 3
    h9_post_core_event(0xFFFF, 0);  // wake app_core (it applies requests after a message)
    td_text(0, 108, "HOLD 3: USB", YELLOW, BLACK);
    td_text(0, 118, "POWER OFF WHEN DONE", WHITE, BLACK);
    uint32_t shown = 0, since = 0;
    int held = 0;
    for (;;) {
        if (h9_key() == TD_KEY_3) {
            if (!held) { held = 1; since = td_ticks(); }
            if (td_ticks() - since >= 200)
                usb_loader();
        } else {
            held = 0;
        }
        if (td_ticks() - shown >= 100) {
            shown = td_ticks();
            td_fill(0, 48, TD_W, 50, BLACK);
            td_text(0, 48, "MODE", WHITE, BLACK);
            number(40, 48, CORE[101], YELLOW);
            td_text(72, 48, "BT", WHITE, BLACK);
            number(96, 48, *(volatile uint32_t *)(0x11AD0 + 360) & 0xFFFFF, YELLOW);
            td_text(0, 62, "HEAP", WHITE, BLACK);
            number(40, 62, HEAP_USED, YELLOW);
            td_text(88, 62, "/", WHITE, BLACK);
            number(96, 62, HEAP_TOTAL, YELLOW);
        }
        h9_delay(10);
    }
}

// Audio probe (hold 4 at power-on; build 14). The BR23 DAC at 0x1E2F00 as
// stock's audio_dac_start (app+0x43434) programs it: +0 CON (|= 0x110 to
// run), +4 ring buffer address, +8 length (samples per channel), +12 0x3C,
// +16 / +20 hardware read / software write pointers, +40 / +44 volume.
// Shows them live with the speaker amplifier byte (0x11B4D bit 0).
//   1: speaker amplifier on (stock gate, source 1)
//   2: square wave into the ring buffer the DAC already has
//   5: start the DAC on our own buffer (DMA bit 0x100 only, no interrupt)
// Stays until power-off; hold 3 for 2 s enters the USB loader.
#define DAC ((volatile uint32_t *)0x1E2F00)
static int16_t probe_ring[1024];

static void hex_at(unsigned x, unsigned y, uint32_t v)
{
    static const char digits[] = "0123456789ABCDEF";
    char s[9];
    for (int i = 0; i < 8; i++)
        s[i] = digits[(v >> (28 - 4 * i)) & 15];
    s[8] = 0;
    td_text(x, y, s, YELLOW, BLACK);
}

static void square(volatile int16_t *ring, unsigned samples)
{
    // about 500 Hz at 16 kHz, half amplitude
    for (unsigned i = 0; i < samples; i++)
        ring[i] = (i / 16) & 1 ? 12000 : -12000;
}

static void audio_probe(void)
{
    static const char *names[] = { "CON", "ADR", "LEN", "C", "HRP", "SWP", "18", "VOL", "2C" };
    static const unsigned regs[] = { 0, 1, 2, 3, 4, 5, 6, 10, 11 };
    td_fill(0, 0, TD_W, TD_H, BLACK);
    td_text(0, 0, "AUDIO PROBE", GREEN, BLACK);
    unsigned last = 0, did = 0;
    uint32_t shown = 0, since = 0;
    for (;;) {
        unsigned key = h9_key();
        if (key != last) {
            last = key;
            since = td_ticks();
            if (key == TD_KEY_1) {
                h9_speaker_gate(1, 1);
                did = 1;
            } else if (key == TD_KEY_2) {
                uint32_t adr = DAC[1], len = DAC[2];
                if (adr >= 0x1000 && adr < 0x30000 && len && len <= 4096)
                    square((volatile int16_t *)adr, len);
                did = 2;
            } else if (key == TD_KEY_5) {
                square(probe_ring, 1024);
                DAC[0] &= ~0x110u;
                DAC[1] = (uint32_t)(uintptr_t)probe_ring;
                DAC[2] = 512;
                DAC[4] = 0;
                DAC[5] = 0;
                DAC[3] = 0x3C;
                DAC[0] |= 0x100;
                did = 5;
            }
        }
        if (key == TD_KEY_3 && td_ticks() - since >= 200)
            usb_loader();
        if (td_ticks() - shown >= 25) {
            shown = td_ticks();
            for (unsigned i = 0; i < 9; i++) {
                unsigned y = 12 + 10 * i;
                td_text(0, y, names[i], WHITE, BLACK);
                hex_at(40, y, DAC[regs[i]]);
            }
            td_text(0, 104, "AMP", WHITE, BLACK);
            hex_at(40, 104, *(volatile uint8_t *)0x11B4D);
            td_text(112, 104, "DID", WHITE, BLACK);
            td_text(144, 104, did == 1 ? "1" : did == 2 ? "2" : did == 5 ? "5" : "-", GREEN, BLACK);
            td_text(0, 116, "1AMP 2SQ 5DAC", WHITE, BLACK);
        }
        h9_delay(10);
    }
}

static char *args[] = { "doom", "-warp", "1", "1", 0 };

void td_main(void)
{
    // 1. The C runtime of the RAM block (the host test's runtime does this).
#ifndef TD_HOST
    memcpy(__data_start, __data_load, (size_t)(__data_end - __data_start));
    memset(__bss_start, 0, (size_t)(__bss_end - __bss_start));
#endif
    h9_led(1);
    td_fill(0, 0, TD_W, TD_H, BLACK);

    // 2. Power-on keys, checked for about 0.3 s: 3 = USB loader, Menu = key
    //    test (shows the stock key byte of each button, to map the controls),
    //    2 = Bluetooth upload mode (after the resource flash set-up),
    //    4 = audio probe.
    int upload_kept = 0;
    for (int i = 0; i < 30; i++) {
        unsigned key = h9_key();
        if (key == TD_KEY_3)
            usb_loader();
        if (key == TD_KEY_MENU)
            key_test();
        if (key == TD_KEY_2)
            upload_kept = 1;
        if (key == TD_KEY_4)
            audio_probe();
        h9_delay(10);
    }

    // 3. Status.
    td_text(0, 0, "H9 DOOM", YELLOW, BLACK);
    td_text(0, 14, "HEAP", WHITE, BLACK);
#ifndef TD_HOST
    number(48, 14, *(volatile uint16_t *)0x1d8a8, WHITE);
    td_text(96, 14, "/", WHITE, BLACK);
    number(104, 14, *(volatile uint16_t *)0x1d8aa, WHITE);
#endif
    uint32_t address, length;
    td_nor_setup();
    // The H9 Toolbox upload service (in this image) and the stock partition
    // reader it uses check the marker stock my_app sets once the resource
    // flash is up; td_nor_setup did that set-up.
#ifndef TD_HOST
    *(volatile uint16_t *)0x12234 = 0x5AA5;
#endif
    if (upload_kept)
        upload_mode("HELD 2");
    int found = td_pack_find(&address, &length);
    if (found)
        upload_mode(found == -1 ? "NO GAME DATA" : found == -2 ? "UPLOAD NOT DONE" : "DATA DAMAGED");
    td_text(0, 28, "PACK", WHITE, BLACK);
    number(48, 28, length, WHITE);

    // 4. The data layer's checks, then the game.
    int e = img_init();
    if (e != IMG_OK) {
        static const char *why[] = { "OK", "FS", "NODATA", "FRAG", "HEADER", "CRC", "DOOMX" };
        td_fatal("PACK CHECK", why[-e <= 6 ? -e : 0]);
    }
    td_text(0, 42, "PACK OK", GREEN, BLACK);

    myargc = 4;
    myargv = args;
    D_DoomMain();
    td_fatal("D_DOOMMAIN", "RETURNED");
}
