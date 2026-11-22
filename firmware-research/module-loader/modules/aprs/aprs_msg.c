/* APRS MSG: received and sent messages. A message detail opens a reply addressed
 * to its sender (or another message to the recipient of an outgoing item).
 * Up/Down select, Menu reads, hold Menu composes, Back returns to stations. */
#include "aprs_core.h"

enum { LIST_Y = BODY_Y, ITEM_H = 22, ITEMS = 4, INFO_Y = FOOT_Y - 10 };

static __attribute__((noinline)) int own_call(const char *call)
{
    const char *mine = A->mycall;
    while (*call && *call == *mine) { ++call; ++mine; }
    return *call == *mine;
}

static const char *target(const struct message *m)
{
    return own_call(m->from) ? m->to : m->from;
}

static void prepare_reply(const struct message *m)
{
    put_str(A->draft_to, target(m));
    A->draft_msg[0] = 0;
    A->draft_valid = 1;
    A->send_sel = 1;                    /* focus the message field */
}

static int matches(const struct message *m, unsigned route)
{
    if (route == H9_MSG_INBOX) return m->mine && !own_call(m->from);
    if (route == H9_MSG_SENT) return own_call(m->from);
    return 1;
}

static unsigned message_order(uint8_t order[MESSAGES], unsigned route)
{
    unsigned n = 0;
    for (unsigned i = 0; i < A->msgs; ++i)
        if (matches(&A->msg[i], route)) order[n++] = i;
    return n;
}

static void draw_list(const uint8_t *order, unsigned count, unsigned selected, unsigned route)
{
    char b[28];
    if (selected < A->msg_top) A->msg_top = selected;
    if (selected >= (unsigned)A->msg_top + ITEMS) A->msg_top = selected - ITEMS + 1;
    aprs_title(route == H9_MSG_INBOX ? "APRS INBOX" : route == H9_MSG_SENT ? "APRS SENT" : "APRS MESSAGES");
    gfx_fill(0, LIST_Y, H9_WIDTH, FOOT_Y - LIST_Y, UI_BG);
    if (!count) aprs_line(LIST_Y + 24, "NO MESSAGES YET", UI_ACCENT, UI_BG);
    for (unsigned k = 0; k < ITEMS && A->msg_top + k < count; k++) {
        unsigned i = A->msg_top + k, y = LIST_Y + k * ITEM_H;
        const struct message *m = &A->msg[order[i]];
        int sel = i == selected;
        uint16_t bg = sel ? UI_ACCENT : UI_BG;
        gfx_fill(0, y, H9_WIDTH, ITEM_H, bg);
        unsigned n = put_str(b, m->mine ? "FROM " : "TO ");
        int sent = own_call(m->from);
        put_str(b + n, sent ? m->to : m->from);
        aprs_line(y, b, sel ? UI_BG : m->mine ? UI_OK : UI_TEXT, bg);
        if (sent)
            gfx_text(H9_WIDTH - 28, y, m->acked ? "ACK" : m->rejected ? "REJ" : "SENT",
                     4, 1, sel ? UI_BG : m->acked ? UI_OK : m->rejected ? UI_DANGER : UI_DIM, bg);
        aprs_line(y + 10, m->text, sel ? UI_BG : m->live ? UI_TEXT : UI_DIM, bg);
    }
    const char *footer = A->mail_status == MAIL_BLOCKED ? "MAILBOX USED BY GAME" :
                         A->mail_status == MAIL_OTHER_OWNER ? "MAILBOX: OTHER CALL" :
                         A->mail_status == MAIL_UNAVAILABLE ? "MAILBOX NOT SAVED" :
                         route ? "BACK  HOLD COMPOSE" :
                         count ? "MENU READ  HOLD COMPOSE" : "HOLD MENU TO COMPOSE";
    aprs_line(INFO_Y, footer, A->mail_status == MAIL_READY ? UI_DIM : UI_DANGER, UI_BG);
    aprs_tabs(TAB_MSG);
}

static void draw_message(void)
{
    const struct message *m = &A->msg[A->msg_sel];
    char b[28];
    aprs_title("MESSAGE");
    gfx_fill(0, TITLE_H, H9_WIDTH, FOOT_Y - TITLE_H, UI_BG);
    unsigned n = put_str(b, "FROM ");
    put_str(b + n, m->from);
    aprs_line(BODY_Y + 2, b, UI_ACCENT, UI_BG);
    n = put_str(b, "TO   ");
    put_str(b + n, m->to);
    aprs_line(BODY_Y + 12, b, m->mine ? UI_OK : UI_TEXT, UI_BG);
    aprs_wrap(BODY_Y + 26, m->text, FOOT_Y - 2, UI_TEXT);
    const char *hint = m->acked ? "ACK RECEIVED  MENU SEND" :
                       m->rejected ? "REJECTED  MENU SEND" :
                       own_call(m->from) ? "NO ACK YET  MENU SEND" :
                       m->mine ? "MENU REPLY  BACK LIST" : "MENU SEND TO  BACK LIST";
    aprs_line(FOOT_Y, hint, m->acked ? UI_OK : m->rejected ? UI_DANGER : UI_HINT, UI_BG);
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)self;
    unsigned route = H9_HAS_MESSAGES ? H9_MSG_ROUTE : 0;
    aprs_open();
    for (;;) {
        unsigned ev = aprs_event();
        A->unread = 0;                          /* the list is on screen */
        if (ev == EV_STORE) { if (aprs_store(api, MAIL_PAGE_MSG)) return 0; continue; }
        if (!A->msg_reading && aprs_tab(api, ev, TAB_MSG)) return 0;
        if (ev == EV_HOLD) {
            if (!A->draft_valid && !A->msg_reading) {
                A->draft_to[0] = A->draft_msg[0] = 0;
                A->draft_valid = 1;          /* a new compose has no guessed addressee */
                A->send_sel = 0;
            }
            if (aprs_go(api, APRS_SEND)) return 0;
            aprs_line(FOOT_Y, "NO SEND APP", UI_DANGER, UI_BG);
            continue;
        }
        if (A->msg_reading) {
            if (ev == EV_MENU) {
                prepare_reply(&A->msg[A->msg_sel]);
                if (aprs_go(api, APRS_SEND)) return 0;
                aprs_line(FOOT_Y, "NO SEND APP", UI_DANGER, UI_BG);
            }
            else if (ev == EV_BACK) { A->msg_reading = 0; A->dirty = 1; }
            else if (ev == EV_REDRAW) draw_message();
            continue;
        }
        uint8_t order[MESSAGES];
        unsigned count = message_order(order, route);
        unsigned selected = 0;
        while (selected + 1 < count && order[selected] != A->msg_sel) ++selected;
        if (ev == EV_REDRAW) draw_list(order, count, selected, route);
        else if (ev == EV_BACK) {
            if (route) aprs_close();
            else aprs_go(api, APRS_LIST);
            return 0;
        } else if (count && (ev == EV_UP || ev == EV_DOWN)) {
            selected = (selected + (ev == EV_UP ? count - 1 : 1)) % count;
            A->msg_sel = order[selected];
            A->dirty = 1;
        } else if (count && ev == EV_MENU) {
            A->msg_sel = order[selected];
            A->msg_reading = 1;
            A->dirty = 1;
        }
    }
}
