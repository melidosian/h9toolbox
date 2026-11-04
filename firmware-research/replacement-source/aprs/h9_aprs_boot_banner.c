#include "h9_aprs_boot_banner.h"
#include "h9_aprs_identity.h"

#include <stddef.h>

/* Preserve the stock banner shape and USART1 ordering without carrying a
 * printf engine; the software version identifies replacement source. */
static const uint8_t replacement_boot_banner[] =
    "888"
    "\r\n========== APRS Module Version Info ==========\r\n"
    "Software Version: " H9_APRS_MODULE_VERSION "\r\n"
    "Hardware Version: " H9_APRS_PROTOCOL_VERSION "\r\n"
    "===========================================\r\n";

bool h9_aprs_boot_banner_send(H9AprsBootWrite write, void *context)
{
    if (write == NULL) return false;
    return write(context, replacement_boot_banner,
                 sizeof replacement_boot_banner - 1u);
}
