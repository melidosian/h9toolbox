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

byte *I_VideoBuffer;

void I_Error(char *error, ...)
{
    td_fatal("ERROR", error);
}

void I_Quit(void)
{
    td_fatal("QUIT", "");
}

byte *I_ZoneBase(int *size)
{
    *size = 0;
    return NULL;
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
}

void I_InitGraphics(void)
{
}

void I_StartFrame(void)
{
}

// TD-H9 keys (stock key byte) to Doom keys. Only one key is reported at a
// time, so a change of key releases the previous one.
static int key_for(unsigned code)
{
    switch (code) {
    case TD_KEY_UP: return KEY_UPARROW;
    case TD_KEY_DOWN: return KEY_DOWNARROW;
    case TD_KEY_MENU: return KEY_LEFTARROW;
    case TD_KEY_BACK: return KEY_RIGHTARROW;
    case TD_KEY_3: return KEY_USE;
    case 0: return 0;
    default: return KEY_FIRE;  // any other key fires
    }
}

void I_StartTic(void)
{
    static int down;
    int key = key_for(h9_key());
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
void I_SetPalette(int palette)
{
    R_PaletteChanged();
    pal_read(palette, pump_lut);
}

void I_FinishUpdate(void)
{
}

void I_InitSound(boolean use_sfx_prefix)
{
    (void)use_sfx_prefix;
}

void I_UpdateSound(void)
{
}

int I_GetSfxLumpNum(sfxinfo_t *sfxinfo)
{
    (void)sfxinfo;
    return 0;
}

int I_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep)
{
    (void)sfxinfo;
    (void)vol;
    (void)sep;
    return channel;
}

void I_StopSound(int handle)
{
    (void)handle;
}

boolean I_SoundIsPlaying(int handle)
{
    (void)handle;
    return false;
}

void I_UpdateSoundParams(int handle, int vol, int sep)
{
    (void)handle;
    (void)vol;
    (void)sep;
}

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

boolean M_Responder(event_t *ev)
{
    return M_OvlResponder(ev);
}

void M_StartControlPanel(void)
{
    M_OvlOpen();
}

void M_Drawer(void)
{
    M_OvlDrawer();
}

volatile pad_t dmc_pad;

int II_PadAxis(int v, int max)
{
    (void)max;
    return v;
}

void snd_volume(int vol)
{
    (void)vol;
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
