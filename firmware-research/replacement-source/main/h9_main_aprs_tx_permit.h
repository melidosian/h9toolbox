#ifndef H9_MAIN_APRS_TX_PERMIT_H
#define H9_MAIN_APRS_TX_PERMIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "h9_main_aprs_metadata.h"

/* Returns zero unless the received module/protocol identity is the exact
 * replacement pair that implements command 0x53. */
size_t h9_main_aprs_tx_permit_encode(const H9MainAprsVersions *versions,
                                      bool enabled, uint16_t token,
                                      uint8_t *output,
                                      size_t capacity);

#endif
