#include "h9_aprs_boot_banner.h"

#include <stddef.h>

/* The stock formatter emits these bytes one at a time to USART1. A single
 * synchronous write keeps their order without carrying a printf engine. */
static const uint8_t stock_boot_banner[] =
    "888"
    "\r\n========== APRS Module Version Info ==========\r\n"
    "Software Version: V1.0.15\r\n"
    "Hardware Version: V1.0.0\r\n"
    "===========================================\r\n";

bool h9_aprs_boot_banner_send(H9AprsBootWrite write, void *context)
{
    if (write == NULL) return false;
    return write(context, stock_boot_banner, sizeof stock_boot_banner - 1u);
}
