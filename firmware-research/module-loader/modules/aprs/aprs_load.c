/* APRS (the menu entry): opens the shared state, loads stock's Beacon List into
 * the station table, then hands over to the station list (".APRS LIST"). It is
 * a separate module so the list, which every other screen returns to, does not
 * carry the loader's code (the screens are close to the RAM slot's size limit). */
#include "aprs_core.h"

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)self;
    if (aprs_open()) {
        /* Start with this callsign's stock Beacon List, then let the mailbox
         * replace it when a committed snapshot exists. */
        A->msgs = A->unread = A->msg_id = A->msg_reading = 0;
        A->msg_sel = A->msg_top = 0;
        A->mail_dirty = 0;
        A->mail_status = MAIL_UNKNOWN;
        aprs_load_stored(api);
    }
    A->mail_action = MAIL_OP_LOAD;
    A->mail_return = MAIL_PAGE_LIST;
    if (!aprs_go(api, APRS_STORE)) {
        A->mail_status = MAIL_UNAVAILABLE;
        aprs_go(api, APRS_LIST);
    }
    return 0;
}
