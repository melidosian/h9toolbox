// SPDX-License-Identifier: GPL-2.0-or-later
// TD-H9 platform for the DMC100-Doom engine (first pass).
//
// Derived from engine/stub/stub_platform.c of DMC100-Doom
// (Copyright (C) 2026 Artur Bieniek, GPL-2.0-or-later): the same set of I_*
// and front-end hooks, with the TD-H9 pieces filled in:
//   - time from the stock 100 Hz tick counter (RAM 0x1D8B0)
//   - input from the stock key scan (key byte, one key at a time)
//   - errors on screen, with the hold-3 USB loader as the way out
// The front end (menus, intermission, finale, saves) stays stubbed in this
// first pass: the game starts straight in E1M1 (-warp 1 1).

#include <stddef.h>

#include "doomtype.h"
#include "doomstat.h"
#include "doomkeys.h"
#include "d_event.h"
#include "d_main.h"
#include "f_finale.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_menu.h"
#include "nor.h"
#include "wi_stuff.h"

#include "dmc_fw.h"
#include "fe.h"
#include "fetch.h"
#include "lcd.h"
#include "ovl.h"
#include "pad.h"
#include "r_strip.h"

#include "td.h"
#include "td_music.h"

byte *I_VideoBuffer;

void I_Error(char *error, ...)
{
    td_fatal("ERROR", error);
}

void I_Quit(void)
{
    td_fatal("QUIT", "");
}

// The zone (engine/z_zone.c, as in DMC100's host build): Z_Init writes its
// header at the base, so it must be real memory (NULL wrote over the RAM code
// at address 0: Doom run 2 hung there). Only small tables are allocated
// (the sound channels of S_Init), so 1 KiB is plenty.
static byte zone[1024] __attribute__((aligned(8)));

byte *I_ZoneBase(int *size)
{
    *size = sizeof zone;
    return zone;
}

void I_Tactile(int on, int off, int total)
{
    (void)on;
    (void)off;
    (void)total;
}

// Doom tics are 1/35 s; the stock counter runs at 100 Hz.
int I_GetTime(void)
{
    td_time_calls++;
    td_trace_counters(0);
    return (int)(td_ticks() * 35u / 100u);
}

int I_GetTimeMS(void)
{
    return (int)(td_ticks() * 10u);
}

void I_Sleep(int ms)
{
    if (ms > 0)
        h9_delay((unsigned)ms);
}

void I_InitTimer(void)
{
}

void I_WaitVBL(int count)
{
    (void)count;
    I_UpdateSound();  // as s_pcspk.h asks: a sound started before a wait plays
}

void I_InitGraphics(void)
{
}

void I_StartFrame(void)
{
}

// TD-H9 keys (stock key byte) to Doom keys, WASD-style on the keypad:
// 2 forward, 5 back, 4 / 6 turn, 1 fire, 3 use; the arrow keys move and turn
// too. Only one key is reported at a time, so a change of key releases the
// previous one.
static int key_for(unsigned code)
{
    switch (code) {
    case TD_KEY_2: case TD_KEY_UP: return KEY_UPARROW;
    case TD_KEY_5: case TD_KEY_DOWN: return KEY_DOWNARROW;
    case TD_KEY_4: case TD_KEY_MENU: return KEY_LEFTARROW;
    case TD_KEY_6: case TD_KEY_BACK: return KEY_RIGHTARROW;
    case TD_KEY_1: return KEY_FIRE;
    case TD_KEY_3: return KEY_USE;
    default: return 0;
    }
}

void I_StartTic(void)
{
    static int down;
    td_tic_calls++;
    td_sound_pump();
    unsigned raw = h9_key();
    static unsigned last_raw;
    if (raw == 9 && last_raw != 9) {  // key 7: music on / off (build 27)
        static int muted;
        muted = !muted;
        td_music_volume(muted ? 0 : 3);
    }
    if (raw == 10 && last_raw != 10) {  // key 8: panel output off / on (build 28)
        extern int td_lcd_skip;
        td_lcd_skip = !td_lcd_skip;
    }
    last_raw = raw;
    int key = key_for(raw);
    if (key == down)
        return;
    event_t ev;
    if (down) {
        ev.type = ev_keyup;
        ev.data1 = down;
        ev.data2 = ev.data3 = 0;
        D_PostEvent(&ev);
    }
    if (key) {
        ev.type = ev_keydown;
        ev.data1 = key;
        ev.data2 = ev.data3 = 0;
        D_PostEvent(&ev);
    }
    down = key;
}

void I_UpdateNoBlit(void)
{
}

// The renderer's palette hook, then the palette into the pump LUT, as the
// device does it.
// Gamma lift for the TD-H9 panel (Doom's palette is dark and the backlight
// is already at stock's top step, duty 10000): out = max * (in / max) ^ 0.6
// per RGB565 channel.
static const uint8_t gamma5[32] = { 0, 4, 6, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 18, 19, 20, 21, 22, 22, 23, 24, 25, 25, 26, 27, 27, 28, 29, 29, 30, 30, 31 };
static const uint8_t gamma6[64] = { 0, 5, 8, 10, 12, 14, 15, 17, 18, 20, 21, 22, 23, 24, 26, 27, 28, 29, 30, 31, 32, 33, 34, 34, 35, 36, 37, 38, 39, 40, 40, 41, 42, 43, 44, 44, 45, 46, 47, 47, 48, 49, 49, 50, 51, 51, 52, 53, 54, 54, 55, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 62, 63 };

void I_SetPalette(int palette)
{
    R_PaletteChanged();
    pal_read(palette, pump_lut);
    for (int i = 0; i < 256; i++) {
        uint16_t c = pump_lut[i];
        pump_lut[i] = (uint16_t)(gamma5[c >> 11] << 11 | gamma6[(c >> 5) & 63] << 5 | gamma5[c & 31]);
    }
}

// Frame-rate readout (build 27): frames in the last second and the slowest
// frame in ms, top-left over the view.
void I_FinishUpdate(void)
{
    pump_sync(0);  // send any held columns before the readout is drawn
    static uint32_t second_start, last_frame, frames, slowest, shown_fps, shown_slow;
    uint32_t now = td_ticks();
    if (last_frame && (now - last_frame) * 10u > slowest)
        slowest = (now - last_frame) * 10u;
    last_frame = now;
    frames++;
    if (now - second_start >= 100) {
        shown_fps = frames * 100u / (now - second_start);
        shown_slow = slowest;
        frames = 0;
        slowest = 0;
        second_start = now;
    }
    char s[8];
    s[0] = (char)('0' + shown_fps / 10 % 10);
    s[1] = (char)('0' + shown_fps % 10);
    s[2] = ' ';
    s[3] = (char)('0' + shown_slow / 100 % 10);
    s[4] = (char)('0' + shown_slow / 10 % 10);
    s[5] = (char)('0' + shown_slow % 10);
    s[6] = 0;
    td_text(0, 0, s, 0xffe0, 0x0000);
}

// Sound: DMC100's port/game/s_pcspk.c (the I_* sound interface) with the
// TD-H9 stepper in td_sound.c.

// Front end: stubbed as in DMC100's size build (no overlays in this pass).
fe_state_t fe;
boolean menuactive;
boolean inhelpscreens;
int messageToPrint;
boolean messageNeedsInput;
int showMessages = 1;
int detailLevel = 1;  // low detail: 160 columns, the TD-H9 panel width
int screenblocks = 10;
dmc_set_t dmc_set;

void M_Init(void)
{
}

void M_Ticker(void)
{
}

// No menu in the first pass: DMC100's menu responder opens the menu on any
// button (its title-loop behaviour), which took every key away from the game.
boolean M_Responder(event_t *ev)
{
    (void)ev;
    return false;
}

void M_StartControlPanel(void)
{
}

void M_Drawer(void)
{
    // no menu in the first pass (DMC100's drawer draws its menu whenever called)
}

volatile pad_t dmc_pad;

int II_PadAxis(int v, int max)
{
    (void)max;
    return v;
}

int settings_save(const void *src, uint32_t size, uint16_t version)
{
    (void)src;
    (void)size;
    (void)version;
    return 0;
}

void fe_frame(int view)
{
    (void)view;
}

void fe_page(void)
{
    F_OvlPage();
}

void ovl_invalidate(void)
{
}

void WI_Start(wbstartstruct_t *wb)
{
    (void)wb;
    WI_OvlStart();
}

void WI_Ticker(void)
{
    WI_OvlTicker();
}

void WI_Drawer(void)
{
    WI_OvlDrawer();
}

void WI_End(void)
{
}

void F_StartFinale(void)
{
    F_OvlStart();
}

boolean F_Responder(event_t *ev)
{
    (void)ev;
    return false;
}

void F_Ticker(void)
{
}

void F_Drawer(void)
{
    F_OvlDrawer();
}

void G_DoSaveGame(void)
{
}

void G_DoLoadGame(void)
{
}

int G_SaveUsable(void)
{
    return SAVE_ENOFILE;
}

int G_SaveHeader(int slot, dmc_save_hdr_t *h)
{
    (void)slot;
    (void)h;
    return SAVE_ENOFILE;
}

int G_SaveCheck(int slot, dmc_save_hdr_t *h)
{
    (void)slot;
    (void)h;
    return SAVE_ENOFILE;
}

void dmc_fatal(const char *msg)
{
    td_fatal("FATAL", msg);
}

int dmc_demo_open(const char *name, uint32_t *size)
{
    (void)name;
    (void)size;
    return -1;
}

void dmc_demo_read(uint32_t off, void *dst, uint32_t n)
{
    (void)off;
    (void)dst;
    (void)n;
}
