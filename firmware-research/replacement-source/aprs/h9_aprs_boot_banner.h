#ifndef H9_APRS_BOOT_BANNER_H
#define H9_APRS_BOOT_BANNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Follows stock 0x08005DE8/0x08005E34 startup order, with a distinct
 * replacement version. The callback must consume bytes before return. */
typedef bool (*H9AprsBootWrite)(void *context, const uint8_t *data,
                                size_t length);

bool h9_aprs_boot_banner_send(H9AprsBootWrite write, void *context);

#endif
