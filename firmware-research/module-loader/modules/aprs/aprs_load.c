/* APRS (the menu entry): opens the shared state, loads stock's Beacon List into
 * the station table, then hands over to the station list (".APRS LIST"). It is
 * a separate module so the list, which every other screen returns to, does not
 * carry the loader's code (the screens are close to the RAM slot's size limit). */
#include "aprs_core.h"

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)self;
    if (aprs_open()) aprs_load_stored(api);
    aprs_go(api, APRS_LIST);         /* if it is missing, back to the menu */
    return 0;
}
