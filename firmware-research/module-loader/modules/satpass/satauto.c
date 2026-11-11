/* .SAT TRACKER: the resident VFO tracker (and its arming), the hidden twin of .SAT TRACK.
 *
 * .SAT TRACK's "start VFO tracking" screen leaves the choices in satshare.h and asks for this
 * module; run normally it arms itself, and the loader stub (src/stub.s res_tick) then calls it
 * as module_main(api, RES_SELF) twice a second from the radio's main loop, so the stock radio
 * keeps running (PTT, squelch, display) while it follows the pass:
 *   listening VFO (A, or B when swapped)  Doppler-corrected downlink (the one it retunes)
 *   sending VFO (the other, if switched on) Doppler-corrected uplink
 * Stock reloads each VFO from its saved record whenever it refreshes the radio (at PTT too), so a
 * frequency only held in RAM is lost and the transmit frequency would be the old one: every update
 * is therefore saved the way the stock stepper saves one (vfo_persist.h), and the user's own
 * frequencies are kept on the chip (vfo_backup.h) and put back when the last pass is over or the
 * menu opens (hold Back). Updates happen when the shift has moved by 400 Hz. It never retunes
 * the receiver onto the sending side. */
#include <stdint.h>

#include "doppler.h"
#include "gfx.h"
#include "h9mod.h"
#include "orbit.h"
#include "satshare.h"
#include "satdata.h"
#include "vfo_persist.h"

#ifndef TICK   /* host tests supply their own */
#define TICK (*(const volatile uint32_t *)0x1D8B0)   /* 100 Hz */
#endif

unsigned module_main(const struct h9_api *api, unsigned self);

/* ---- resident tracker ---------------------------------------------------- */

#ifndef SAT_HOST   /* the host test supplies its own hardware access */
#define STOCK 0x11AD0u                                   /* radio state block (0x11AD0 + offset) */
#define VFO_A (*(volatile uint32_t *)(STOCK + 0x1D0))    /* 10 Hz units */
#define VFO_B (*(volatile uint32_t *)(STOCK + 0x1D8))
#define RX_FREQ (*(volatile uint32_t *)(STOCK + 0x1BC))  /* the selected side's working copy */
#define SIDE (*(volatile int8_t *)(STOCK + 0x80))        /* negative: VFO B is selected */
#define BAND (*(volatile uint8_t *)(STOCK + 0x10CD))     /* 1 above 300 MHz */
#define UI_DIRTY (*(volatile uint16_t *)(0xC0F0u + 0x46))
#define STOCK_RETUNE() ((void (*)(void))0x01E595B6)()    /* tune receive from RX_FREQ */
#define STOCK_REDRAW() ((void (*)(unsigned))0x01E53306)(0)   /* what the stepper calls after a change: marks the 3 display rows for redraw */
#define RF_READ(reg) ((unsigned (*)(unsigned))0x01E505FE)(reg)
#define STOCK_OPEN() ((void (*)(void))0x01E83B42)()      /* the receive state machine's "squelch opened": REG 0x30 = 0xBFF5, speaker gate on */
#define TX_TONE (*(volatile uint16_t *)(STOCK + 0x9A))    /* the selected side's working transmit tone (two BCD bytes) */
#define STOCK_REFRESH() ((void (*)(unsigned))0x01E58E3E)(0)  /* full RF refresh: reloads both VFOs from their saved records */
#define PF1_SHORT (*(volatile uint8_t *)(STOCK + 0x10EC))   /* the PF1 short-press action (setting 0xC91 in the settings partition: 0 none, 7 PTT2) */
#define SQUELCH (*(volatile uint16_t *)(STOCK + 0x83C))   /* the receive state machine's open threshold (0x01E840BC compares the signal with it) */
#endif
enum { BOOT_SECONDS = 4, LEAD = 60, MOVE = 40 /* 400 Hz */ };
#define TX_ON 0x4000u   /* RF register 0x30 while transmitting (stock writes 0xC1FE; receive values have this bit clear) */

/* Which side is selected: the working copy is the selected side's word, so compare it with the two
 * words (the side byte at +0x80 only breaks a tie; its polarity is unconfirmed on hardware). */
static unsigned active_side(void)
{
    uint32_t cur = RX_FREQ;
    if (cur == VFO_B && cur != VFO_A) return 1;
    if (cur == VFO_A && cur != VFO_B) return 0;
    return SIDE < 0;
}

static uint32_t get_vfo(unsigned side) { return side ? VFO_B : VFO_A; }

/* ---- countdown on the radio's own screen -------------------------------------------------
 * The status bar has a gap between the "S" (signal) icon and the Bluetooth icon (screen x 60-112,
 * y 2-16, from a photo of the stock screen): an 8-character label fits. It reads AOS m:ss until the
 * rise and LOS m:ss until the set; flag 8 (test now) shows TEST. It is drawn on a small black box
 * every call (stock may redraw the bar), and when it stops being shown the box is blanked and
 * stock is asked to redraw its rows (flag 128 = drawn). */
enum { TIMER_RIGHT = 112, TIMER_Y = 4 };

static char *put(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *digits(char *d, uint32_t v, unsigned width)
{
    char t[10];
    unsigned n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 10);
    while (n < width) t[n++] = '0';
    while (n) *d++ = t[--n];
    *d = 0;
    return d;
}

static void show_timer(volatile struct satshare *sh, uint32_t now, uint32_t aos, uint32_t los)
{
    char b[12], *d = b;
    uint16_t color = GFX_WHITE;
    if (sh->st_flags & 8) d = put(d, "TEST");
    else if (now < aos && aos - now < 6000) { d = put(d, "AOS "); d = digits(d, (aos - now) / 60, 1); *d++ = ':'; d = digits(d, (aos - now) % 60, 2); }
    else if (now >= aos && now <= los) { d = put(d, "LOS "); d = digits(d, (los - now) / 60, 1); *d++ = ':'; d = digits(d, (los - now) % 60, 2); color = GFX_GREEN; }
    else {
        if (sh->st_flags & 128) {   /* nothing to show any more: blank the box and let stock redraw */
            gfx_fill(TIMER_RIGHT - 6 * 9 - 2, TIMER_Y - 1, 6 * 9 + 4, 10, GFX_BLACK);
            STOCK_REDRAW();
            sh->st_flags &= ~128u;
        }
        return;
    }
    unsigned len = (unsigned)(d - b);
    unsigned x = TIMER_RIGHT - 6 * len;
    gfx_fill(x - 2, TIMER_Y - 1, 6 * len + 4, 10, GFX_BLACK);
    gfx_text(x, TIMER_Y, b, len, 1, color, GFX_BLACK);
    sh->st_flags |= 128;
}

/* PF1 short press = PTT2 while tracking (flag 512 asks for it): stock then sends the main PTT on VFO A,
 * so with B listening and A sending the PTT key always transmits on A. The setting is only changed in RAM
 * (the radio reads it again from its settings at the next restart); the original is kept (orig_pf1, flag
 * 256) and put back when tracking ends. Stock's own default for this key is PTT2 (value 7). */
enum { PTT2 = 7 };
static void pf1_ptt2(volatile struct satshare *sh, unsigned on)
{
    if (on && (sh->st_flags & 512)) {
        if (!(sh->st_flags & 256)) { sh->orig_pf1 = PF1_SHORT; sh->st_flags |= 256; }
        PF1_SHORT = PTT2;
    } else if (sh->st_flags & 256) {
        PF1_SHORT = (uint8_t)sh->orig_pf1;
        sh->st_flags &= ~256u;
    }
}

/* Hold the squelch open while the pass is on: the receive state machine (0x01E840BC) opens when its
 * measure reaches the halfword at +0x83C, so 0 never closes it; the radio recomputes that halfword
 * from the squelch setting only when it refreshes, and the next call puts the 0 back. The original is
 * kept (orig_sq) and restored when the pass is over. Flag 32 asks for it, flag 64 says it is saved. */
static void squelch(volatile struct satshare *sh, unsigned in_pass)
{
    if (in_pass && (sh->st_flags & 32)) {
        if (!(sh->st_flags & 64)) { sh->orig_sq = SQUELCH; sh->st_flags |= 64; }
        SQUELCH = 0;
    } else if (sh->st_flags & 64) {
        SQUELCH = (uint16_t)sh->orig_sq;
        sh->st_flags &= ~64u;
    }
}

/* Put a frequency into VFO A (side 0) or B (side 1). The selected side's working copy follows. Only the
 * listening side (`retune`) is retuned: the receiver must never be moved onto the sending side's frequency
 * (it is the one you transmit on, and the transmit-in-progress check is not trusted yet: RF register 0x30
 * read 0 on the radio), so that frequency is picked up by the stock code the next time it tunes (PTT or
 * a side change). */
static unsigned put_vfo(unsigned side, uint32_t f10, unsigned retune)
{
    unsigned active = active_side();   /* before the word changes */
    vfo_save(side, f10);               /* stock reloads the saved record at its next refresh (PTT too) */
    if (side) VFO_B = f10; else VFO_A = f10;
    STOCK_REDRAW();                    /* both rows are drawn from the words: the unselected side's row too */
    if (active != side) return 0;      /* the other side picks its word up when selected */
    /* the stepper's tail without its "changed" flag: word, redraw, working copy, tune. (With that
     * flag set, stock reloaded the saved VFO from its settings record about a second later and
     * put the old frequency back.) */
    BAND = f10 > 29999999u;
    RX_FREQ = f10;
    if (retune) STOCK_RETUNE();
    return retune;
}

/* The sending side's TX tone for the satellite (two BCD bytes; 0xFFFF = none): saved in the VFO's record so the
 * refresh at PTT loads it, and set in the working copy when that side is selected. */
static void set_tx_tone(unsigned side, uint16_t code)
{
    vfo_save_tone(side, code);
    if (active_side() == side) TX_TONE = code;
}

static unsigned moved(uint32_t a, uint32_t b) { return a > b ? a - b >= MOVE : b - a >= MOVE; }

static void tracker(const struct h9_api *api)
{
    volatile struct satshare *sh = SAT_SHARED;
    uint32_t tick = TICK;
    sh->st_ticks++;
    if (tick < sh->tick) { sh->now += BOOT_SECONDS; sh->tick = tick; }   /* the tick restarted with the radio */
    uint32_t now = sh->now + (tick - sh->tick) / 100u;
    unsigned n = 0;
    while (n < sh->count && sh->aos[n] + (sh->dur_sat[n] & 0xFFF) + 30 < now) n++;
    pf1_ptt2(sh, 1);                   /* from the first call after the restart until the last pass is over */
    if (n >= sh->count) { sh->res_magic = 0; sh->st_flags |= 4; pf1_ptt2(sh, 0); squelch(sh, 0); show_timer(sh, now, 0, 0); vfo_restore_pending(api->read); STOCK_REFRESH(); return; }  /* every listed pass is over: your squelch and VFOs back */
    uint32_t aos = sh->aos[n], los = aos + (sh->dur_sat[n] & 0xFFF);
    show_timer(sh, now, aos, los);
    squelch(sh, (sh->st_flags & 8) || (now >= aos && now <= los));   /* open from the rise to the set (flag 8: test now) */
    if (((sh->st_flags & 8) == 0 && now + LEAD < aos) || (RF_READ(0x30) & TX_ON)) { sh->st_flags |= 4; return; }   /* flag 8: test now, whatever the time */
    sh->st_flags &= ~4u;
    /* stock reloads a VFO word from its saved record whenever it refreshes the radio: if it put the old
     * value back, write ours again (and count it, for VFO PROBE) */
    unsigned lis = (sh->st_flags >> 4) & 1, snd = lis ^ 1;   /* flag 16: listen on VFO B, send on VFO A */
    if (sh->st_down && get_vfo(lis) != sh->st_down) sh->st_reverts++;
    if ((sh->st_flags & 2) && sh->st_up && get_vfo(snd) != sh->st_up) sh->st_reverts++;
    struct satdata data;
    struct satentry entry;
    struct gmst_ref gmst;
    if (!satdata_open(api, &data)) return;            /* the satellite table is missing: leave the radio alone */
    unsigned id = sh->dur_sat[n] >> 12;
    id = id < data.head.count ? id : 0;
    satdata_get(&data, id, &entry);
    satdata_gmst(&data, &gmst);
    const struct satfreq *f = &entry.freq;
    uint32_t t = now < aos ? aos : now > los ? los : now;
    struct observer o;
    struct look before, after;
    orbit_observer(&o, sh->lat, sh->lon);
    orbit_look(&entry.sat, &gmst, &o, t - DOPPLER_HALF, &before);
    orbit_look(&entry.sat, &gmst, &o, t + DOPPLER_HALF, &after);
    int32_t span = after.range - before.range;
    if ((sh->st_flags & 2) && sh->spare != id + 1) { set_tx_tone(snd, vfo_tone_code(f->tone10)); sh->spare = (uint8_t)(id + 1); }   /* the satellite's access tone, once per satellite */
    uint32_t down = (uint32_t)((int32_t)f->down10 - doppler_shift(f->down10, span));
    uint32_t up = (uint32_t)((int32_t)f->up10 + doppler_shift(f->up10, span));
    /* put the same value back when stock overwrote it; a new one only when the shift has moved */
    unsigned retuned = 0;
    if (sh->st_down && get_vfo(lis) != sh->st_down) retuned = put_vfo(lis, sh->st_down, 1);
    else if (!sh->st_down || moved(down, sh->st_down)) { retuned = put_vfo(lis, down, 1); sh->st_down = down; }
    /* the receive tune switches the audio off (REG 0x30 = 0xBDF1) and stock only opens it again when its
     * state machine sees the squelch change from closed to open: with the squelch held open that never
     * happens, so the sound stayed off after every frequency change. Open it as stock does. */
    if (retuned && (sh->st_flags & 64)) STOCK_OPEN();
    if (sh->st_flags & 2) {
        if (sh->st_up && get_vfo(snd) != sh->st_up) put_vfo(snd, sh->st_up, 0);
        else if (!sh->st_up || moved(up, sh->st_up)) { put_vfo(snd, up, 0); sh->st_up = up; }
    }
}


/* Arm the tracker for the pass list in satshare (the choices are in st_flags) and restart the radio. */
static void arm(const struct h9_api *api, unsigned self)
{
    volatile struct satshare *sh = SAT_SHARED;
    struct h9_entry e;
    api->read(&e, api->bundle + H9_BUNDLE_HEADER + H9_BUNDLE_ENTRY * self, sizeof e);
    /* the user's own VFOs and TX tones, once: if an earlier tracker was never stopped, those are still the originals */
    uint32_t a = VFO_A, b = VFO_B;
    uint16_t ta = vfo_read_tone(0), tb = vfo_read_tone(1);
    unsigned lis = (sh->st_flags >> 4) & 1, snd = lis ^ 1, send = (sh->st_flags & 2) != 0;
    unsigned touched = (lis ? VFO_B_TOUCHED : VFO_A_TOUCHED) | (send ? (snd ? VFO_B_TOUCHED : VFO_A_TOUCHED) | (snd ? VFO_B_TONE : VFO_A_TONE) : 0);
    unsigned pending = vfo_backup_load(api->read, &a, &b, &ta, &tb);
    vfo_backup_save(api->read, a, b, ta, tb, (pending & ~VFO_ARMED) | touched | VFO_ARMED);
    sh->spare = 0;                       /* no satellite's tone applied yet */
    sh->res_entry = (uint32_t)(uintptr_t)module_main;
    sh->res_lencrc = e.size << 16 | e.crc;
    sh->res_tick = 0;
    sh->st_ticks = 0; sh->st_down = sh->st_up = 0;
    sh->st_valid = RES_MAGIC;
    sh->st_reverts = 0;
    sh->st_flags &= ~(64u | 128u);
    sh->res_magic = RES_MAGIC;
    h9_reset();
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    if (self == RES_SELF) { tracker(api); return 0; }   /* called by the loader stub while the radio runs */
    volatile struct satshare *sh = SAT_SHARED;
    if (sh->magic != SAT_MAGIC || !sh->count || sh->count > 5) return 0;   /* not asked for by .SAT TRACK */
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, GFX_BLACK);
    gfx_text(0, 50, "STARTING VFO TRACKING", 21, 1, GFX_WHITE, GFX_BLACK);
    arm(api, self);
    return 0;
}
