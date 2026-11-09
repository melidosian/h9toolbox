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

// No complete pack: wait while the H9 Toolbox upload page writes one over
// Bluetooth (prototypes/data-upload), checking the directory every 3 s.
static void wait_for_upload(int why, uint32_t *address, uint32_t *length)
{
    td_fill(0, 0, TD_W, TD_H, BLACK);
    td_text(0, 0, "H9 DOOM", YELLOW, BLACK);
    td_text(0, 20, why == -2 ? "UPLOAD NOT DONE" : why == -1 ? "NO GAME DATA" : "DATA DAMAGED", RED, BLACK);
    td_text(0, 40, "WAITING FOR", WHITE, BLACK);
    td_text(0, 52, "BLUETOOTH UPLOAD", WHITE, BLACK);
    td_text(0, 108, "HOLD 3: USB", YELLOW, BLACK);
    uint32_t checked = td_ticks(), since = 0, dots = 0;
    int held = 0;
    for (;;) {
        if (h9_key() == TD_KEY_3) {
            if (!held) { held = 1; since = td_ticks(); }
            if (td_ticks() - since >= 200)
                usb_loader();
        } else {
            held = 0;
        }
        if (td_ticks() - checked >= 300) {
            checked = td_ticks();
            td_fill(0, 72, TD_W, 10, BLACK);
            td_text(8 * (dots++ % 18), 72, ".", GREEN, BLACK);
            if (!td_pack_find(address, length)) {
                td_text(0, 88, "DATA FOUND", GREEN, BLACK);
                h9_delay(1000);
                td_fill(0, 0, TD_W, TD_H, BLACK);
                td_text(0, 0, "H9 DOOM", YELLOW, BLACK);
                return;
            }
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
    //    test (shows the stock key byte of each button, to map the controls).
    for (int i = 0; i < 30; i++) {
        unsigned key = h9_key();
        if (key == TD_KEY_3)
            usb_loader();
        if (key == TD_KEY_MENU)
            key_test();
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
    // Bluetooth diagnostic (build 9): the system core's mode state (stock,
    // running: mode 3, +360 set, 0xC11A = 98), shown for 6 s.
#ifndef TD_HOST
    {
        volatile uint8_t *core = (volatile uint8_t *)0x11AD0;
        td_text(0, 56, "MODE", WHITE, BLACK);
        number(40, 56, core[101], YELLOW);
        td_text(72, 56, "REQ", WHITE, BLACK);
        number(104, 56, core[100], YELLOW);
        td_text(0, 70, "P360", WHITE, BLACK);
        uint32_t p = *(volatile uint32_t *)(0x11AD0 + 360);
        number(40, 70, p & 0xFFFFF, YELLOW);
        td_text(0, 84, "C11A", WHITE, BLACK);
        number(40, 84, *(volatile uint8_t *)0xC11A, YELLOW);
        td_text(72, 84, "B108", WHITE, BLACK);
        number(112, 84, core[108], YELLOW);
        td_text(0, 98, "B120", WHITE, BLACK);
        number(40, 98, core[120], YELLOW);
        h9_delay(6000);
        td_fill(0, 56, TD_W, 52, BLACK);
    }
#endif
    uint32_t address, length;
    td_nor_setup();
    // The H9 Toolbox upload service (in this image) and the stock partition
    // reader it uses check the marker stock my_app sets once the resource
    // flash is up; td_nor_setup did that set-up.
#ifndef TD_HOST
    *(volatile uint16_t *)0x12234 = 0x5AA5;
#endif
    int found = td_pack_find(&address, &length);
    if (found)
        wait_for_upload(found, &address, &length);
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
