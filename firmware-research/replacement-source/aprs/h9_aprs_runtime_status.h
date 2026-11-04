#ifndef H9_APRS_RUNTIME_STATUS_H
#define H9_APRS_RUNTIME_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#include "h9_aprs_runtime_state.h"
#include "h9_aprs_status.h"

/* Stock command-0x13 byte layout filled from replacement runtime telemetry.
 * Individual counter meanings can differ from stock; see aprs-status.md. */
bool h9_aprs_runtime_status_read(const H9AprsRuntimeState *state,
                                 uint8_t output[H9_APRS_STATUS_BYTES]);

#endif
