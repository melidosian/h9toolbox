/* APRS SEND (module "APRS SEND": in the module menu, by holding Menu on
 * APRS MSG, or with Menu on a station's details):
 * send one packet from your callsign through stock's own KISS transmit
 * routine 0x01E5DBCC, which keys, sends, waits the airtime and unkeys with
 * stock's own checks (resource-flash-log.md 2026-11-10). H9 Toolbox never
 * drives the transmitter itself.
 * Items: TO (recipient, typed; starts as the station selected in the list),
 * MSG (typed), SEND MESSAGE, a STATUS test, and QUICK texts that fill MSG.
 * Up/Down choose; Menu: edit TO/MSG, or review, then Menu again: transmit;
 * Back: cancel / list; hold Menu: station list.
 * Typing (aprs_edit.c): digits multi-tap (2 = ABC2 … 0 = space 0,
 * 1 = .,?!-/@1), Back deletes, Menu or Up/Down finish. The draft lives in this
 * screen's stack: leaving SEND drops it.
 * Review switches to the APRS TX CH side (Lock and Auto alike; A+B keeps the
 * current side); leaving review returns to the APRS RX CH side. */
#include "aprs_core.h"
#include "aprs_edit.h"
#include "aprs_tx.h"


static const char STATUS[] = "H9 Toolbox test";
static const char *const QUICK[] = { "QSL 73", "TEST, PLEASE ACK" };
enum { I_TO, I_MSG, I_SEND, I_STATUS, I_QUICK, ITEMS = I_QUICK + 2 };
enum { LIST_Y = BODY_Y + 22, ROW_H = 12, SHOW = 21, ADDRESSEE = 9 };
enum { CHOOSE, EDIT, REVIEW, DONE };

/* Info field for item i into out; returns why it cannot be sent, or 0. */
static const char *info_of(unsigned i, char *out, const struct edit *to, const struct edit *msg)
{
    if (i == I_STATUS) { out[0] = '>'; put_str(out + 1, STATUS); return 0; }
    if (!to->len) return "ENTER A STATION (TO)";
    if (!msg->len) return "ENTER A MESSAGE (MSG)";
    unsigned n = 0, k = 0;
    out[n++] = ':';
    for (; k < ADDRESSEE && to->buf[k]; k++) out[n++] = to->buf[k];
    for (; k < ADDRESSEE; k++) out[n++] = ' ';
    out[n++] = ':';
    n += put_str(out + n, msg->buf);
    out[n++] = '{';
    put_uint(out + n, A->msg_id % 99 + 1);
    return 0;
}

/* A sent message at the top of the MSG list (from you, white, not "new").
 * Heard back through a digipeater it is not listed twice: the receive side
 * skips a message with the same sender and text. */
static void sent(const char *to, const char *text, unsigned id)
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
    m->acked = 0;
    m->id = (uint8_t)id;                    /* the other station's ack carries it */
    A->msg_sel = 0;
}

static const char *problem(unsigned i, char *info, uint8_t *kiss, unsigned *len,
                           const struct edit *to, const struct edit *msg)
{
    const char *why = info_of(i, info, to, msg);
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

/* Row i: label and value; while editing, the value's tail and a cursor. */
static void draw_row(unsigned i, int editing, const struct edit *to, const struct edit *msg)
{
    static const char *const LABEL[ITEMS] = { "TO ", "MSG ", "SEND MESSAGE", "STATUS ", "QUICK ", "QUICK " };
    char b[32];
    unsigned y = LIST_Y + i * ROW_H;
    int sel = i == A->send_sel;
    uint16_t bg = editing ? GFX_GREEN : sel ? GFX_YELLOW : GFX_BLACK;
    unsigned n = put_str(b, LABEL[i]);
    const char *v = i == I_TO ? to->buf : i == I_MSG ? msg->buf : i == I_STATUS ? STATUS
                  : i >= I_QUICK ? QUICK[i - I_QUICK] : "";
    unsigned vlen = 0;
    while (v[vlen]) vlen++;
    unsigned room = SHOW - n;                       /* the end of a long value stays visible */
    if (vlen > room) v += vlen - room;
    n += put_str(b + n, v);
    if (editing) b[n++] = '_', b[n] = 0;
    else if ((i == I_TO || i == I_MSG) && !vlen) put_str(b + n, "-");
    gfx_fill(0, y, H9_WIDTH, ROW_H, bg);
    gfx_text(4, y + 2, b, 25, 1, sel || editing ? GFX_BLACK : GFX_WHITE, bg);
}

static void draw_choose(int editing, const struct edit *to, const struct edit *msg)
{
    header();
    for (unsigned i = 0; i < ITEMS; i++) draw_row(i, editing && i == A->send_sel, to, msg);
    aprs_line(FOOT_Y, editing ? "0-9 TYPE BACK DEL MENU OK" : "MENU: SELECT  HOLD: LIST", GFX_GREY, GFX_BLACK);
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
    struct edit to, msg;
    unsigned state = CHOOSE, len = 0;
    const char *why = 0;
    (void)self;
    aprs_open();
    edit_init(&to, ADDRESSEE, 1, A->count ? A->st[A->list_sel].call : 0);
    edit_init(&msg, EDIT_MAX, 0, 0);
    if (A->send_sel >= ITEMS) A->send_sel = I_TO;
    for (;;) {
        unsigned ev = aprs_event();
        if (ev == EV_HOLD) { if (aprs_go(api, APRS_LIST)) return 0; continue; }
        if (ev == EV_REDRAW) {
            if (state == REVIEW) draw_review(why, info);
            else if (state != DONE) draw_choose(state == EDIT, &to, &msg);
            continue;
        }
        if (state == EDIT) {
            struct edit *e = A->send_sel == I_TO ? &to : &msg;
            if (ev >= EV_DIGIT && ev <= EV_DIGIT + 9) edit_digit(e, ev - EV_DIGIT, TICK);
            else if (ev == EV_BACK) edit_back(e);
            else {                                  /* Menu, Up or Down: done */
                state = CHOOSE;
                if (ev == EV_MENU) A->send_sel = A->send_sel == I_TO ? I_MSG : I_SEND;
                else if (ev == EV_UP) A->send_sel = A->send_sel ? A->send_sel - 1 : ITEMS - 1;
                else if (ev == EV_DOWN) A->send_sel = A->send_sel + 1 < ITEMS ? A->send_sel + 1 : 0;
            }
            A->dirty = 1;
            continue;
        }
        if (state == REVIEW && ev == EV_MENU && !why) {
            header();
            aprs_line(BODY_Y + 40, "TRANSMITTING...", GFX_RED, GFX_BLACK);
            ((void (*)(const uint8_t *, unsigned))STOCK_KISS_TX)(kiss, len);
            aprs_quiet();                           /* stock's end of TX turns the speaker on */
            unsigned id = A->msg_id % 99 + 1;       /* the {id that info_of put in the packet */
            A->msg_id++;
            if (A->send_sel == I_SEND) {
                sent(to.buf, msg.buf, id);
                edit_init(&msg, EDIT_MAX, 0, 0);    /* sent: start the next one empty */
            }
            header();                               /* stock may have drawn over us */
            aprs_line(BODY_Y + 30, "HANDED TO THE RADIO", GFX_GREEN, GFX_BLACK);
            aprs_wrap(BODY_Y + 44, info, FOOT_Y - 2, GFX_WHITE);
            aprs_line(FOOT_Y, "ANY KEY: BACK", GFX_GREY, GFX_BLACK);
            A->last_key = h9_key();
            state = DONE;
            continue;
        }
        if (state != CHOOSE) {                      /* review or result: back to the items */
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
            unsigned i = A->send_sel;
            if (i == I_TO || i == I_MSG) {
                state = EDIT;
            } else if (i >= I_QUICK) {
                edit_init(&msg, EDIT_MAX, 0, QUICK[i - I_QUICK]);
                A->send_sel = I_SEND;
            } else {
                aprs_side(aprs_tx_side());          /* the TX check reads this side */
                info[0] = 0;
                why = problem(i, info, kiss, &len, &to, &msg);
                state = REVIEW;
            }
            A->dirty = 1;
        } else if (ev >= EV_DIGIT && ev <= EV_DIGIT + 9 && (A->send_sel == I_TO || A->send_sel == I_MSG)) {
            /* Typing on TO or MSG starts editing it, replacing TO's default. */
            struct edit *e = A->send_sel == I_TO ? &to : &msg;
            if (e == &to) edit_init(&to, ADDRESSEE, 1, 0);
            edit_digit(e, ev - EV_DIGIT, TICK);
            state = EDIT;
            A->dirty = 1;
        }
    }
}
