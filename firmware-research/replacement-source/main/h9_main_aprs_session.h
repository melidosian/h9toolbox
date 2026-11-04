#ifndef H9_MAIN_APRS_SESSION_H
#define H9_MAIN_APRS_SESSION_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "h9_main_aprs_control.h"
#include "h9_main_aprs_metadata.h"
#include "h9_main_gps_fix.h"
#include "h9_main_uart_ring.h"

/* Callbacks must consume or copy supplied buffers before returning. The
 * session owns all storage and may reuse it on the next frame. */
typedef struct {
    void (*packet)(void *context, const H9MainAx25Decoded *packet);
    void (*gps_sentence)(void *context, const uint8_t *sentence,
                         size_t length);
    void (*gps_fix)(void *context, H9MainGpsSentenceType type,
                    const H9MainGpsFix *fix);
    void (*control)(void *context, H9MainControlAction action);
    /* Replacement APRS-controller activity signal; does not gate audio. */
    void (*burst_activity)(void *context, bool active);
    /* Replacement-only completion of the final APRS DAC sample. */
    void (*tx_drained)(void *context, uint16_t token);
    void (*version)(void *context, const H9MainAprsVersions *versions);
    void (*serial_write)(void *context, const uint8_t *bytes, size_t length);
    /* Returns bytes copied into destination, never more than capacity. */
    size_t (*serial_read)(void *context, uint8_t *destination, size_t capacity);
} H9MainAprsSessionCallbacks;

typedef struct {
    H9MainUartRing uart_ring;
    uint8_t serial_read_buffer[H9_MAIN_UART_DRAIN_LIMIT];
    uint32_t serial_read_faults;
    bool py_ready;
    H9MainWireReceiver receiver;
    H9MainAprsVersions versions;
    H9MainGpsCollector gps;
    H9MainGpsFix gps_fix;
    H9MainAx25Decoded decoded;
    H9MainAprsSessionCallbacks callbacks;
    void *callback_context;
} H9MainAprsSession;

void h9_main_aprs_session_init(H9MainAprsSession *session,
                               const H9MainAprsSessionCallbacks *callbacks,
                               void *context);

/* If serial_read is supplied, one poll asks the board adapter for up to
 * 1,024 bytes, pushes them into the ring, then drains at most 1,024 bytes.
 * A callback that reports more than its capacity is rejected and counted.
 * No hardware registers, audio gates, or RF transmit path are touched here. */
size_t h9_main_aprs_session_poll(H9MainAprsSession *session);

/* Stock startup display consumes the PY 02 01 55 ready flag once and
 * clears it after handling the event. The UI loop can call this when it
 * reaches that stage; polling UART alone must not discard the event. */
bool h9_main_aprs_session_take_ready(H9MainAprsSession *session);

#endif
