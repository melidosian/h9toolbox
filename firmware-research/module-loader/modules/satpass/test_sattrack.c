/* Host test for the resident VFO tracker in satauto.c (tracker()): which
 * words and saved records it writes, when, and when it must keep its hands off the radio. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define module_main track_main   /* before h9mod.h: its prototype is the one satauto.c takes the address of */
#include "h9mod.h"
#include "gfx.h"

#define SAT_HOST
static struct { uint32_t a, b, rx; int8_t side; uint8_t band; } radio;
static uint16_t host_dirty;
static uint16_t host_squelch = 38;   /* the radio's own threshold */
static uint8_t host_pf1 = 5;           /* the radio's own PF1 short-press action */
#define PF1_SHORT host_pf1
#define SQUELCH host_squelch
static uint32_t host_tick = 1000;
static unsigned host_reg30, retunes;
#define VFO_A radio.a
#define VFO_B radio.b
#define RX_FREQ radio.rx
#define SIDE radio.side
#define BAND radio.band
#define UI_DIRTY host_dirty
#define TICK host_tick
#define STOCK_RETUNE() (retunes++)
#define STOCK_REDRAW() (redraws++)
#define STOCK_OPEN() (opens++)
static unsigned opens;
static unsigned redraws;
#define RF_READ(reg) (host_reg30)
#define H9_TZ_BYTE host_tz
static unsigned char host_tz = 12;
/* the stock record write (vfo_persist.h) and the resource chip (vfo_backup.c) on the host */
static struct { unsigned part, offset, len; uint8_t data[4]; } saves[64];
static unsigned nsaves;
#define STOCK_ENCODE(f, buf) do { uint32_t _f = (f); memcpy(buf, &_f, 4); } while (0)   /* the host keeps the plain number */
#define STOCK_PARTITION_WRITE(p_, o_, src_, n_) do { saves[nsaves].part = (p_); saves[nsaves].offset = (o_); saves[nsaves].len = (n_); memcpy(saves[nsaves].data, (src_), 4); nsaves++; } while (0)
static uint8_t host_scratch[16];
static uint16_t host_tone_record[2] = { 0x0885, 0x0885 };   /* the saved TX tones of A and B before tracking (88.5 Hz) */
static uint16_t host_txtone = 0x0885;
#define TX_TONE host_txtone
#define STOCK_REFRESH() (refreshes++)
static unsigned refreshes;
#define STOCK_PARTITION_READ(p_, o_, dst_, n_) do { uint16_t _t = host_tone_record[((o_) - 10) == 0x1960]; memcpy((dst_), &_t, 2); } while (0)
#define VFO_SCRATCH ((void *)host_scratch)
static uint8_t host_chip[4096];
static void host_erase(uint32_t sector) { (void)sector; memset(host_chip, 0xFF, sizeof host_chip); }
static void host_program(const void *src, uint32_t address, uint32_t length) { for (uint32_t i = 0; i < length; i++) host_chip[address - 0x3FE000u + i] &= ((const uint8_t *)src)[i]; }
#define NOR_ERASE host_erase
#define NOR_PROGRAM host_program
static void host_read(void *dst, uint32_t address, uint32_t length) { memcpy(dst, host_chip + (address - 0x3FE000u), length); }
static const struct h9_api *host_apip;   /* a fake bundle with .SAT DATA (host_satdata.h); the backup sector goes to host_read */
#include "satshare.h"
static struct satshare host_share;
#undef SAT_SHARED
#define SAT_SHARED (&host_share)
static struct h9_request host_request;
#undef H9_REQUEST
#define H9_REQUEST (&host_request)

static unsigned fills;
void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; fills++; }
static char shown[16]; static unsigned shown_x, shown_y; static uint16_t shown_color;
unsigned gfx_text(unsigned x, unsigned y, const char *t, unsigned m, unsigned s, uint16_t f, uint16_t b) { (void)s; (void)b; unsigned i; for (i = 0; i < m && t[i]; i++) shown[i] = t[i]; shown[i] = 0; shown_x = x; shown_y = y; shown_color = f; return x + m; }
void gfx_wait_release(void) {}
unsigned gfx_wait_key(void) { return 0; }
unsigned h9_key(void) { return 0; }
void h9_delay(unsigned u) { (void)u; }
void h9_reset(void) {}

#include "host_satdata.h"
#include "vfo_backup.c"
#include "satauto.c"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

/* PO-101 (table index 4) over Chicago: rise 1794309009, 597 s (see orbit_cli) */
#define AOS 1794309009u
#define PO101_DOWN 14590000u
#define PO101_UP 43750000u

static void scene(uint32_t now_unix, unsigned flags)
{
    memset(&host_share, 0, sizeof host_share);
    memset(&radio, 0, sizeof radio);
    host_pf1 = 5; refreshes = 0; host_txtone = 0x0885; opens = 0; host_dirty = 0; host_squelch = 38; retunes = 0; redraws = 0; nsaves = 0; memset(host_chip, 0xFF, sizeof host_chip); host_reg30 = 0xBFF5; host_tick = 1000;
    host_share.lat = (int32_t)(41878000LL * 4294967296LL / 360000000LL);
    host_share.lon = (int32_t)(-87630000LL * 4294967296LL / 360000000LL);
    host_share.now = now_unix; host_share.tick = 1000;
    host_share.count = 2;
    host_share.aos[0] = AOS; host_share.dur_sat[0] = (uint16_t)(4 << 12 | 597);
    host_share.aos[1] = AOS + 6000; host_share.dur_sat[1] = (uint16_t)(4 << 12 | 500);
    radio.a = 14580000; radio.b = 43500000; radio.rx = radio.a;   /* the user's own VFOs; A selected */
    host_share.res_magic = RES_MAGIC;
    host_share.st_flags = flags;
}

int main(void)
{
    host_other_read = host_read;
    host_apip = host_satdata_api(getenv("SATDATA") ? getenv("SATDATA") : "build/satdata.bin");
    /* 400 s into the pass, A selected, listen only: receding, so below the base frequency */
    scene(AOS + 400, 0);
    tracker(host_apip);
    CHECK(radio.a < PO101_DOWN && radio.a > PO101_DOWN - 400);   /* 1.4 kHz = 140 units */
    CHECK(radio.b == 43500000 && host_share.st_up == 0);         /* B untouched */
    CHECK(radio.rx == radio.a && radio.band == 0 && retunes == 1 && redraws == 1 && host_dirty == 0);   /* no "changed" flag: stock would reload the saved VFO */
    CHECK(host_share.st_down == radio.a && host_share.st_ticks == 1 && !(host_share.st_flags & 4));
    uint32_t saved; memcpy(&saved, saves[0].data, 4);
    CHECK(nsaves == 1 && saves[0].part == 4 && saves[0].offset == 0x1950 && saves[0].len == 4 && saved == radio.a);   /* saved like a dial step: stock keeps it */
    uint32_t first = radio.a;
    tracker(host_apip);                                                   /* same second: no change worth a retune */
    CHECK(radio.a == first && retunes == 1);
    host_tick += 100 * 90; tracker(host_apip);                            /* 90 s later: the shift has moved by more than 400 Hz */
    CHECK(radio.a != first && retunes == 2);

    /* stock puts the saved VFO A back: the next call writes ours again and counts it */
    scene(AOS + 400, 0); tracker(host_apip);
    uint32_t mine = radio.a; radio.a = 14580000; radio.rx = radio.a;
    tracker(host_apip);
    CHECK(radio.a == mine && host_share.st_reverts == 1 && host_dirty == 0);   /* the same value, not a fresh one */

    /* transmit side on: B gets the uplink, above its base */
    scene(AOS + 400, 2);
    tracker(host_apip);
    CHECK(radio.b > PO101_UP && radio.b < PO101_UP + 800 && radio.band == 0);   /* A selected: B's word only */
    CHECK(nsaves == 3 && saves[2].offset == 0x1960 && saves[0].len == 2);          /* B's tone, A's record, then B's record */
    CHECK(retunes == 1);                                         /* only A's retune: B is not the selected side */
    /* B selected (found by comparing the working copy with the words, whatever the side byte says): the
     * working copy and band follow B, A keeps its word, and the receiver is never retuned onto B */
    scene(AOS + 400, 2); radio.rx = radio.b; radio.side = 0x7F;
    tracker(host_apip);
    CHECK(radio.rx == radio.b && radio.b > PO101_UP && radio.band == 1 && radio.a < PO101_DOWN && retunes == 0);
    /* both words equal: the side byte breaks the tie */
    scene(AOS + 400, 0); radio.b = radio.a; radio.side = -1;
    tracker(host_apip);
    CHECK(radio.rx == radio.b || retunes == 0);

    /* the unselected side's row is redrawn too: stock draws the rows from the words */
    scene(AOS + 400, 2); tracker(host_apip);
    CHECK(redraws == 2 && retunes == 1);                          /* A (selected, listening) and B (send side, not selected) */

    /* swapped: listen on VFO B, send on VFO A (flag 16) */
    scene(AOS + 400, 16);                                           /* B listens, A selected */
    tracker(host_apip);
    CHECK(radio.b < PO101_DOWN && radio.b > PO101_DOWN - 400 && radio.a == 14580000 && retunes == 0);   /* B (not selected) gets the downlink word, no retune */
    scene(AOS + 400, 16); radio.rx = radio.b;                       /* B listens and is selected */
    tracker(host_apip);
    CHECK(radio.rx == radio.b && radio.b < PO101_DOWN && retunes == 1 && radio.band == 0);
    CHECK(nsaves == 1 && saves[0].offset == 0x1960);              /* B listens: B's record */
    scene(AOS + 400, 18);                                           /* swapped, send side on: A gets the uplink word, never a retune */
    tracker(host_apip);
    CHECK(radio.a > PO101_UP && radio.a < PO101_UP + 800 && radio.b < PO101_DOWN && retunes == 0);
    scene(AOS + 400, 18); radio.rx = radio.b;                       /* ... B selected and listening: retuned once; A's word is set but not selected */
    tracker(host_apip);
    CHECK(retunes == 1 && radio.a > PO101_UP);

    /* squelch held open during the pass (flag 32), the original kept and put back when it is over */
    scene(AOS + 400, 32); tracker(host_apip);
    CHECK(host_squelch == 0 && host_share.orig_sq == 38 && (host_share.st_flags & 64));
    host_squelch = 41; tracker(host_apip);                          /* stock refreshed and put a threshold back */
    CHECK(host_squelch == 0 && host_share.orig_sq == 38);          /* held again, the original not replaced */
    scene(AOS - 30, 32); tracker(host_apip);                        /* before the rise: not yet */
    CHECK(host_squelch == 38 && !(host_share.st_flags & 64));
    scene(AOS + 90000, 32); host_share.orig_sq = 38; host_share.st_flags |= 64; host_squelch = 0; tracker(host_apip);   /* last pass over */
    CHECK(host_squelch == 38 && host_share.res_magic == 0);
    scene(AOS + 400, 0); tracker(host_apip);
    CHECK(host_squelch == 38);                                      /* off by default in the test scenes: untouched */

    /* the countdown in the status bar's gap: AOS before the rise, LOS during, nothing after */
    scene(AOS + 400, 0); memset(shown, 0, sizeof shown); tracker(host_apip);
    CHECK(!strcmp(shown, "LOS 3:17") && shown_color == GFX_GREEN && (host_share.st_flags & 128));   /* 597 - 400 = 197 s */
    CHECK(shown_x + 6 * strlen(shown) == 112 && shown_y == 4);                                      /* right-aligned in the gap */
    scene(AOS - 135, 0); memset(shown, 0, sizeof shown); tracker(host_apip);
    CHECK(!strcmp(shown, "AOS 2:15") && shown_color == GFX_WHITE);
    scene(AOS - 20000, 0); memset(shown, 0, sizeof shown); tracker(host_apip);                      /* more than 99 minutes away: not shown */
    CHECK(shown[0] == 0 && !(host_share.st_flags & 128));
    scene(AOS - 3000, 8); memset(shown, 0, sizeof shown); tracker(host_apip);
    CHECK(!strcmp(shown, "TEST"));
    scene(AOS + 400, 0); tracker(host_apip); scene(AOS + 598, 0); host_share.st_flags |= 128; unsigned f0 = fills; redraws = 0; tracker(host_apip);   /* the pass just ended */
    CHECK(fills > f0 && redraws >= 1 && !(host_share.st_flags & 128));                              /* blanked and stock asked to redraw */

    /* every retune with the squelch held open also reopens the audio (stock's own open routine) */
    scene(AOS + 400, 32); tracker(host_apip);
    CHECK(retunes == 1 && opens == 1);
    scene(AOS + 400, 0); tracker(host_apip);
    CHECK(retunes == 1 && opens == 0);                              /* squelch not held: left to stock */
    scene(AOS + 400, 32 | 16); tracker(host_apip);                  /* B listens, A selected: no retune, nothing to reopen */
    CHECK(retunes == 0 && opens == 0);

    /* PF1 short press = PTT2 while tracking (flag 512), the original kept and put back after the last pass */
    scene(AOS - 20000, 512); tracker(host_apip);                       /* even before the pass window */
    CHECK(host_pf1 == 7 && host_share.orig_pf1 == 5 && (host_share.st_flags & 256));
    host_pf1 = 2; scene(AOS - 20000, 512 | 256); host_share.orig_pf1 = 5; host_pf1 = 7; tracker(host_apip);
    CHECK(host_pf1 == 7 && host_share.orig_pf1 == 5);                   /* kept, not replaced by 7 */
    scene(AOS + 90000, 512 | 256); host_share.orig_pf1 = 5; host_pf1 = 7; tracker(host_apip);
    CHECK(host_pf1 == 5 && !(host_share.st_flags & 256));              /* the last pass is over: back */
    scene(AOS + 400, 0); tracker(host_apip);
    CHECK(host_pf1 == 5);                                              /* not asked for: untouched */

    /* the satellite's access tone on the sending side, saved in its VFO record (bytes 10-11), once per satellite */
    scene(AOS + 400, 2 | 16 * 0); tracker(host_apip);               /* PO-101: 141.3 Hz, send on B (A listens) */
    { unsigned n_tone = 0, off = 0; for (unsigned i = 0; i < nsaves; i++) if (saves[i].len == 2) { n_tone++; off = saves[i].offset; }
      CHECK(n_tone == 1 && off == 0x1960 + 10); }
    { unsigned i = 0; while (i < nsaves && saves[i].len != 2) i++; CHECK(i < nsaves && saves[i].data[0] == 0x13 && saves[i].data[1] == 0x14); }   /* 141.3 -> BCD 1413 -> bytes 13 14 */
    CHECK(host_txtone == 0x0885);                                      /* B is not the selected side: the working tone stays */
    tracker(host_apip);                                               /* same satellite: not written again */
    { unsigned n = 0; for (unsigned i = 0; i < nsaves; i++) if (saves[i].len == 2) n++; CHECK(n == 1); }
    scene(AOS + 400, 2 | 16); tracker(host_apip);                      /* swapped: A sends */
    { unsigned i = 0; while (i < nsaves && saves[i].len != 2) i++; CHECK(i < nsaves && saves[i].offset == 0x1950 + 10 && host_txtone == 0x1413); }   /* A selected: the working tone follows */
    scene(AOS + 400, 0); tracker(host_apip);                           /* send off: no tone written */
    { unsigned n = 0; for (unsigned i = 0; i < nsaves; i++) if (saves[i].len == 2) n++; CHECK(n == 0); }

    /* never retune while transmitting */
    scene(AOS + 400, 2); host_reg30 = 0xC1FE;
    tracker(host_apip);
    CHECK(radio.a == 14580000 && radio.b == 43500000 && retunes == 0 && (host_share.st_flags & 4));

    /* a minute before the rise it preloads the rise frequencies, earlier it waits */
    scene(AOS - 120, 0); tracker(host_apip);
    CHECK(radio.a == 14580000 && (host_share.st_flags & 4));
    scene(AOS - 30, 0); tracker(host_apip);
    CHECK(radio.a > PO101_DOWN && radio.a < PO101_DOWN + 1500);   /* at the rise the satellite is approaching: heard above its base */

    /* test mode (flag 8): acts at once, at the rise frequencies, listen only */
    scene(AOS - 3000, 0); host_share.st_flags |= 8; tracker(host_apip);
    CHECK(radio.a > PO101_DOWN && radio.b == 43500000 && retunes == 1);

    /* after the first pass it moves to the next; after the last it disarms */
    scene(AOS + 597 + 100, 0); tracker(host_apip);
    CHECK(host_share.res_magic == RES_MAGIC && radio.a == 14580000 && (host_share.st_flags & 4));   /* next pass is 5400 s away */
    scene(AOS + 90000, 0);
    vfo_backup_save(host_read, 14580000, 43500000, 0x0885, 0xFFFF, VFO_ARMED | VFO_A_TOUCHED | VFO_A_TONE);   /* the tracker had changed A and its TX tone */
    tracker(host_apip);
    CHECK(host_share.res_magic == 0);
    uint32_t back; memcpy(&back, saves[0].data, 4);
    CHECK(nsaves == 2 && saves[0].offset == 0x1950 && back == 14580000 && saves[1].offset == 0x1950 + 10 && saves[1].data[0] == 0x85 && saves[1].data[1] == 0x08);   /* the user's VFO A and its tone put back, B left alone */
    CHECK(refreshes == 1);                                                       /* and stock reloads its VFOs */
    { uint32_t a = 0, b = 0; uint16_t ta = 0, tb = 0; CHECK(vfo_backup_load(host_read, &a, &b, &ta, &tb) == 0 && a == 14580000 && ta == 0x0885); }   /* and nothing pending */
    nsaves = 0; vfo_restore_pending(host_read); CHECK(nsaves == 0);             /* a second restore does nothing */

    /* the tick restarted with the radio: the clock is moved on by the boot time, not left behind */
    scene(AOS + 400, 0); host_share.tick = 500000; host_tick = 300;
    tracker(host_apip);
    CHECK(host_share.tick == 300 && host_share.now == AOS + 400 + BOOT_SECONDS);

    printf(failures ? "%d failure(s)\n" : "sattrack ok\n", failures);
    return failures != 0;
}
