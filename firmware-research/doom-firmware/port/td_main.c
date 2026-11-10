// SPDX-License-Identifier: GPL-2.0-or-later
// TD-H9 port of DMC100-Doom: the program entry, called by the startup in the
// app cave (td_entry.s) once the panel is up. Lives in the far region.
//
// 1. .data copied from flash, .bss cleared (the engine's fixed core lives in
//    the RAM block taken from the end of the stock heap).
// 2. Power-on keys: 3 enters the mask-ROM USB loader (ROM 0x02000D68, the
//    mode hold-8 gives on stock); 2 enters Bluetooth upload mode.
// 3. The pack found by the upload directory (no complete pack: upload mode),
//    img_init (the data layer's header and section checks), sound and music,
//    then D_DoomMain with -warp 1 1.
// A failure stays on screen; hold 3 for 2 s then enters the USB loader.

#include <stdint.h>
#include <string.h>

#include "doomtype.h"
#include "d_main.h"
#include "img.h"
#include "m_argv.h"
#include "s_pcspk.h"
#include "dmcdat.h"

#include "td.h"
#include "td_music.h"
#include "nor.h"

extern uint8_t __data_load[], __data_start[], __data_end[], __bss_start[], __bss_end[];

enum { BLACK = 0x0000, WHITE = 0xffff, YELLOW = 0xffe0, RED = 0xf800, GREEN = 0x07e0, CYAN = 0x07ff };

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

// Busy-loop iterations per 100 Hz tick over 25 ticks: C-hello's LOOP, the CPU
// speed estimate (H9 Toolbox: about 200,000 = 240 MHz; 24 MHz would read
// about 20,000). Diagnostic (build 33): slow upload-mode Bluetooth, and the
// game's own clock.
uint32_t td_cpu_loops(void)
{
    volatile uint32_t loops = 0;
    uint32_t first = td_ticks();
    while (td_ticks() == first) {
    }
    first = td_ticks();
    while (td_ticks() - first < 25 && loops < 200000000u)
        loops++;
    return loops / 25;
}

static void number(unsigned x, unsigned y, uint32_t v, uint16_t color)
{
    char t[11];
    int i = 10;
    t[i] = 0;
    do {
        t[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v && i);
    td_text(x, y, t + i, color, BLACK);
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
    // Wait style (key 5 cycles; release 32 test of the slow upload: build 26
    // uploaded at 123 B/s here against 785 B/s in H9 Toolbox, reads were
    // about 6x slower too): Y yields 10 ms, B never yields, L yields 200 ms.
    static const char styles[] = "EFYBL";
    unsigned style = 0, last = 0, last_six = 0, clock_set = 0;
    uint32_t since = 0, shown = 1, cpu = td_cpu_loops();  // measured once (a busy loop)
    for (;;) {
        unsigned key = h9_key();
        if (key != last) {
            last = key;
            since = td_ticks();
            if (key == TD_KEY_5) {
                style = (style + 1) % 5;
                shown = 1;
            }
        }
        if (key == TD_KEY_3 && td_ticks() - since >= 200)
            usb_loader();
        if (key == TD_KEY_6 && last_six != TD_KEY_6) {
            h9_clk_set_sys(240000000u);  // build 33: stock's clk_set("sys", 240 MHz)
            clock_set = 1;
            cpu = td_cpu_loops();
            shown = 1;
        }
        last_six = key;
        if (shown) {
            char text[8] = { 'W', 'A', 'I', 'T', ' ', styles[style], 0 };
            td_fill(0, 48, TD_W, 38, BLACK);
            td_text(0, 48, text, CYAN, BLACK);
            td_text(0, 62, "5 WAIT  6 240MHZ", WHITE, BLACK);
            td_text(0, 76, "CPU", WHITE, BLACK);
            number(32, 76, cpu, YELLOW);
            if (clock_set)
                td_text(112, 76, "SET", GREEN, BLACK);
            shown = 0;
        }
        // E (default, build 34): wake app_core every 10 ms with the ignored
        // 0xFFFF event, in case the Bluetooth replies wait for its loop
        // (build 33: CPU at about 135,000 loops per tick, so not a 24 MHz
        // clock; each exchange still takes about a second). Y yields 10 ms,
        // B never yields, L yields 200 ms.
        // Build 34 on hardware: E read at about 1.3 KiB/s (about 0.5 before).
        // F (build 35): an event about every 2 ms (a short spin between, sized
        // from the measured loop rate), yielding once per tick.
        if (style == 0) {
            h9_post_core_event(0xFFFF, 0);
            h9_delay(10);
        } else if (style == 1) {
            for (unsigned i = 0; i < 4; i++) {
                h9_post_core_event(0xFFFF, 0);
                for (volatile uint32_t spin = cpu / 5; spin; spin--) {
                }
            }
            h9_post_core_event(0xFFFF, 0);
            h9_delay(10);
        } else if (style == 2) {
            h9_delay(10);
        } else if (style == 4) {
            h9_delay(200);
        }
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

    // 2. Power-on keys, checked for about 0.3 s: 3 = USB loader, 2 =
    //    Bluetooth upload mode (after the resource flash set-up).
    int upload_kept = 0;
    for (int i = 0; i < 30; i++) {
        unsigned key = h9_key();
        if (key == TD_KEY_3)
            usb_loader();
        if (key == TD_KEY_2)
            upload_kept = 1;
        h9_delay(10);
    }

    // 3. Status.
    td_text(0, 0, "H9 DOOM", YELLOW, BLACK);
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

    // 4. The data layer's checks, then the game.
    int e = img_init();
    if (e != IMG_OK) {
        static const char *why[] = { "OK", "FS", "NODATA", "FRAG", "HEADER", "CRC", "DOOMX" };
        td_fatal("PACK CHECK", why[-e <= 6 ? -e : 0]);
    }
    td_text(0, 20, "GAME DATA OK", GREEN, BLACK);
    // Build 33 diagnostic: the CPU before and after stock's clk_set("sys",
    // 240 MHz); the game then runs at whatever that leaves.
    td_text(0, 62, "CPU", WHITE, BLACK);
    number(32, 62, td_cpu_loops(), YELLOW);
    h9_clk_set_sys(240000000u);
    td_text(0, 76, "240", WHITE, BLACK);
    number(32, 76, td_cpu_loops(), GREEN);
    h9_delay(3000);

    // 5. Sound: the pack's PC-speaker effects through the DAC (td_sound.c).
    S_PcsInit(img_read, img.sect[DMC_SECT_SOUNDS].off, img.sect[DMC_SECT_SOUNDS].size);
    td_sound_init();
    td_text(0, 34, img.sect[DMC_SECT_SOUNDS].size ? "SOUND ON" : "NO SOUNDS", GREEN, BLACK);
    // 6. Music: an H9MU song in the music slot (doom-firmware/music.py).
    {
        uint32_t song, song_length;
        int music = td_dir_find(TD_MUSIC_DIRECTORY, &song, &song_length);
        if (!music)
            music = td_music_start(nor_read, song, song_length);
        td_text(0, 48, music ? "NO MUSIC" : "MUSIC ON", music ? WHITE : GREEN, BLACK);
    }

    myargc = 4;
    myargv = args;
    D_DoomMain();
    td_fatal("D_DOOMMAIN", "RETURNED");
}
