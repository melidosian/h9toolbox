/* APRS MSG: APRS messages heard (module "APRS MSG"), receive only.
 * Messages to your callsign are green and counted as new until viewed.
 * Messages I sent show ACK in green once the other station acknowledged
 * them. Up/Down select, Menu: read, 1-5 or hold Menu: other screens (APRS
 * SEND to write one), Back: station list. */
#include "aprs_core.h"

enum { LIST_Y = BODY_Y, ITEM_H = 22, ITEMS = 4, INFO_Y = FOOT_Y - 10 };

static void draw_list(void)
{
    char b[28];
    if (A->msg_sel < A->msg_top) A->msg_top = A->msg_sel;
    if (A->msg_sel >= A->msg_top + ITEMS) A->msg_top = A->msg_sel - ITEMS + 1;
    aprs_title("MSG");
    gfx_fill(0, LIST_Y, H9_WIDTH, FOOT_Y - LIST_Y, GFX_BLACK);
    if (!A->msgs) aprs_line(LIST_Y + 24, "NO MESSAGES YET", GFX_YELLOW, GFX_BLACK);
    for (unsigned k = 0; k < ITEMS && A->msg_top + k < A->msgs; k++) {
        unsigned i = A->msg_top + k, y = LIST_Y + k * ITEM_H;
        const struct message *m = &A->msg[i];
        int sel = i == A->msg_sel;
        unsigned n = put_str(b, m->from);
        b[n++] = '>';
        put_str(b + n, m->to);
        aprs_line(y, b, sel ? GFX_BLACK : m->mine ? GFX_GREEN : GFX_WHITE, sel ? GFX_YELLOW : GFX_BLACK);
        if (m->acked) gfx_text(H9_WIDTH - 22, y, "ACK", 3, 1, sel ? GFX_BLACK : GFX_GREEN, sel ? GFX_YELLOW : GFX_BLACK);
        for (n = 0; n < 26 && m->text[n]; n++) b[n] = m->text[n];
        b[n] = 0;
        aprs_line(y + 10, b, m->live ? GFX_WHITE : GFX_GREY, GFX_BLACK);
    }
    unsigned n = put_str(b, "ME ");
    n += put_str(b + n, A->mycall[0] ? A->mycall : "? (SET IN RADIO)");
    if (A->msgs) put_str(b + n, "  MENU:READ");
    aprs_line(INFO_Y, b, GFX_GREY, GFX_BLACK);
    aprs_tabs(TAB_MSG);
}

static void draw_message(void)
{
    const struct message *m = &A->msg[A->msg_sel];
    char b[28];
    aprs_title("MSG");
    gfx_fill(0, TITLE_H, H9_WIDTH, FOOT_Y - TITLE_H, GFX_BLACK);
    unsigned n = put_str(b, "FROM ");
    put_str(b + n, m->from);
    aprs_line(BODY_Y + 2, b, GFX_YELLOW, GFX_BLACK);
    n = put_str(b, "TO   ");
    put_str(b + n, m->to);
    aprs_line(BODY_Y + 12, b, m->mine ? GFX_GREEN : GFX_WHITE, GFX_BLACK);
    aprs_wrap(BODY_Y + 26, m->text, FOOT_Y - 2, GFX_WHITE);
    aprs_line(FOOT_Y, m->acked ? "DELIVERED (ACK)  BACK" : m->live ? "HEARD LIVE   BACK: LIST" : "FROM STOCK LIST  BACK", m->acked ? GFX_GREEN : GFX_GREY, GFX_BLACK);
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    unsigned reading = 0;
    (void)self;
    aprs_open();
    for (;;) {
        unsigned ev = aprs_event();
        A->unread = 0;                          /* the list is on screen */
        if (!reading && aprs_tab(api, ev, TAB_MSG)) return 0;
        if (ev == EV_HOLD) {
            if (aprs_go(api, APRS_SEND)) return 0;
            aprs_line(FOOT_Y, "NO SEND APP", GFX_RED, GFX_BLACK);
            continue;
        }
        if (reading) {
            if (ev == EV_BACK || ev == EV_MENU) { reading = 0; A->dirty = 1; }
            else if (ev == EV_REDRAW) draw_message();
            continue;
        }
        if (ev == EV_REDRAW) draw_list();
        else if (ev == EV_BACK) { aprs_go(api, APRS_LIST); return 0; }
        else if (!A->msgs) continue;
        else if (ev == EV_UP) { A->msg_sel = A->msg_sel ? A->msg_sel - 1 : A->msgs - 1; A->dirty = 1; }
        else if (ev == EV_DOWN) { A->msg_sel = A->msg_sel + 1 < A->msgs ? A->msg_sel + 1 : 0; A->dirty = 1; }
        else if (ev == EV_MENU) { reading = 1; A->dirty = 1; }
    }
}
