/* Hardware test for the stub's ml_reset (not an app). Installed as the whole
 * bundle (build/test-ml-reset-bundle.bin, via the App Manager's "Load whole
 * bundle"), it stands in for the menu and returns 0 at once, which the stub
 * treats as "back to the radio": ml_reset. Expected: hold Back -> the radio
 * restarts into the normal channel screen (the old ROM 0x0200000A exit froze).
 * Afterwards update the menu and apps in the App Manager again. */
#include "h9mod.h"

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)api; (void)self;
    return 0;
}
