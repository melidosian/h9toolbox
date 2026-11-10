/* APRS SEND (module "APRS SEND": in the module menu, by holding Menu on
 * APRS MSG, or with Menu on a station's details):
 * send one canned packet from your callsign through stock's own KISS transmit
 * routine 0x01E5DBCC, which keys, sends, waits the airtime and unkeys with
 * stock's own checks (resource-flash-log.md 2026-11-10). H9 Toolbox never
 * drives the transmitter itself.
 * Up/Down choose, Menu: review, Menu again: transmit, Back: cancel / list,
 * hold Menu: station list.
 * Items: a status test, then messages to the station selected in the list.
 * Review switches to the APRS TX CH side (Lock and Auto alike; A+B keeps the
 * current side); leaving review returns to the APRS RX CH side. */
#include "aprs_core.h"
#include "aprs_tx.h"

#define STOCK_KISS_TX 0x01E5DBCCu
#define TX_FREQ (*(const volatile uint32_t *)0x11C9C)     /* working TX, 10 Hz units */
enum { TX_LOW = 14400000, TX_HIGH = 14800000 };          /* 2 m band only */

static const char *const CANNED[] = { "H9 Toolbox test", "Hello from H9 Toolbox", "QSL 73", "Test, please ack" };
enum { ITEMS = 4, LIST_Y = BODY_Y + 34, ROW_H = 12 };
enum { CHOOSE, REVIEW, DONE };

/* Info field for item i into out; returns why it cannot be sent, or 0. */
static const char *info_of(unsigned i, char *out)
{
    if (i == 0) { out[0] = '>'; put_str(out + 1, CANNED[0]); return 0; }   /* status */
    if (!A->count) return "NO STATION SELECTED";
    const char *to = A->st[A->list_sel].call;
    unsigned n = 0, k = 0;
    out[n++] = ':';
    for (; k < 9 && to[k]; k++) out[n++] = to[k];
    for (; k < 9; k++) out[n++] = ' ';
    out[n++] = ':';
    n += put_str(out + n, CANNED[i]);
    out[n++] = '{';
    put_uint(out + n, A->msg_id % 99 + 1);
    return 0;
}

/* A sent message at the top of the MSG list (from you, white, not "new").
 * Heard back through a digipeater it is not listed twice: the receive side
 * skips a message with the same sender and text. */
static void sent(const char *to, const char *text)
{
    for (unsigned i = MESSAGES - 1; i > 0; i--) A->msg[i] = A->msg[i - 1];
    if (A->msgs < MESSAGES) A->msgs++;
    struct message *m = &A->msg[0];
    unsigned n = 0;
    for (; n < CALL - 1 && A->mycall[n]; n++) m->from[n] = A->mycall[n];
    m->from[n] = 0;
    for (n = 0; n < CALL - 1 && to[n]; n++) m->to[n] = to[n];
    m->to[n] = 0;
    for (n = 0; n < MSG_TEXT - 1 && text[n]; n++) m->text[n] = text[n];
    m->text[n] = 0;
    m->live = 1;
    m->mine = 0;
    A->msg_sel = 0;
}

static const char *problem(unsigned i, char *info, uint8_t *kiss, unsigned *len)
{
    const char *why = info_of(i, info);
    if (why) return why;
    if (!A->mycall[0]) return "NO CALLSIGN SET";
    if (TX_FREQ < TX_LOW || TX_FREQ > TX_HIGH) return "TX NOT ON 2M";
    *len = aprs_tx_kiss(kiss, A->mycall, APRS_TX_DEST, APRS_TX_PATH, info);
    return *len ? 0 : "BAD CALLSIGN";
}

static void header(void)
{
    char b[28];
    aprs_title("SEND");
    gfx_fill(0, TITLE_H, H9_WIDTH, FOOT_Y - TITLE_H, GFX_BLACK);
    unsigned n = put_str(b, "FROM ");
    put_str(b + n, A->mycall[0] ? A->mycall : "?");
    aprs_line(BODY_Y, b, GFX_GREEN, GFX_BLACK);
    n = put_str(b, "TX ");
    n += aprs_tx_mhz(b + n, TX_FREQ);
    put_str(b + n, " MHZ");
    aprs_line(BODY_Y + 10, b, TX_FREQ >= TX_LOW && TX_FREQ <= TX_HIGH ? GFX_WHITE : GFX_YELLOW, GFX_BLACK);
}

static void draw_choose(void)
{
    char b[28];
    header();
    unsigned n = put_str(b, "MSG TO ");
    put_str(b + n, A->count ? A->st[A->list_sel].call : "-");
    aprs_line(BODY_Y + 20, b, GFX_GREY, GFX_BLACK);
    for (unsigned i = 0; i < ITEMS; i++) {
        unsigned y = LIST_Y + i * ROW_H;
        int sel = i == A->send_sel;
        uint16_t bg = sel ? GFX_YELLOW : GFX_BLACK;
        gfx_fill(0, y, H9_WIDTH, ROW_H, bg);
        n = put_str(b, i ? "MSG " : "STATUS ");
        put_str(b + n, CANNED[i]);
        gfx_text(4, y + 2, b, 25, 1, sel ? GFX_BLACK : GFX_WHITE, bg);
    }
    aprs_line(FOOT_Y, "MENU: REVIEW  HOLD: LIST", GFX_GREY, GFX_BLACK);
}

static void draw_review(const char *why, const char *info)
{
    header();
    aprs_line(BODY_Y + 20, "TO " APRS_TX_DEST " VIA " APRS_TX_PATH, GFX_GREY, GFX_BLACK);
    aprs_wrap(BODY_Y + 34, info, FOOT_Y - 14, GFX_WHITE);
    if (why) {
        aprs_line(FOOT_Y - 12, why, GFX_RED, GFX_BLACK);
        aprs_line(FOOT_Y, "BACK: CHANGE", GFX_GREY, GFX_BLACK);
    } else {
        aprs_line(FOOT_Y, "MENU: TRANSMIT  BACK: NO", GFX_YELLOW, GFX_BLACK);
    }
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    char info[APRS_TX_INFO_MAX + 1];
    uint8_t kiss[APRS_TX_KISS_MAX];
    unsigned state = CHOOSE, len = 0;
    const char *why = 0;
    (void)self;
    aprs_open();
    if (A->send_sel >= ITEMS) A->send_sel = 0;
    for (;;) {
        unsigned ev = aprs_event();
        if (ev == EV_HOLD) { if (aprs_go(api, APRS_LIST)) return 0; continue; }
        if (ev == EV_REDRAW) {
            if (state == CHOOSE) draw_choose();
            else if (state == REVIEW) draw_review(why, info);
            continue;
        }
        if (state == REVIEW && ev == EV_MENU && !why) {
            header();
            aprs_line(BODY_Y + 40, "TRANSMITTING...", GFX_RED, GFX_BLACK);
            ((void (*)(const uint8_t *, unsigned))STOCK_KISS_TX)(kiss, len);
            aprs_quiet();                           /* stock's end of TX turns the speaker on */
            A->msg_id++;
            if (A->send_sel) sent(A->st[A->list_sel].call, CANNED[A->send_sel]);
            header();                               /* stock may have drawn over us */
            aprs_line(BODY_Y + 30, "HANDED TO THE RADIO", GFX_GREEN, GFX_BLACK);
            aprs_wrap(BODY_Y + 44, info, FOOT_Y - 2, GFX_WHITE);
            aprs_line(FOOT_Y, "ANY KEY: BACK", GFX_GREY, GFX_BLACK);
            A->last_key = h9_key();
            state = DONE;
            continue;
        }
        if (state != CHOOSE) {                      /* review or result: back to the list */
            if (ev == EV_BACK || ev == EV_MENU || state == DONE) {
                aprs_side(aprs_rx_side());          /* back to the APRS RX channel */
                state = CHOOSE;
                A->dirty = 1;
            }
            continue;
        }
        if (ev == EV_BACK) { aprs_go(api, APRS_LIST); return 0; }
        else if (ev == EV_UP) { A->send_sel = A->send_sel ? A->send_sel - 1 : ITEMS - 1; A->dirty = 1; }
        else if (ev == EV_DOWN) { A->send_sel = A->send_sel + 1 < ITEMS ? A->send_sel + 1 : 0; A->dirty = 1; }
        else if (ev == EV_MENU) {
            aprs_side(aprs_tx_side());              /* the TX check reads this side */
            info[0] = 0;
            why = problem(A->send_sel, info, kiss, &len);
            state = REVIEW;
            A->dirty = 1;
        }
    }
}
