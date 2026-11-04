#ifndef H9_MAIN_APRS_SESSION_H
#define H9_MAIN_APRS_SESSION_H

#include <stddef.h>
#include <stdint.h>

#include "h9_main_aprs_control.h"
#include "h9_main_aprs_metadata.h"
#include "h9_main_uart_ring.h"

/* Callbacks must consume or copy supplied buffers before returning. The
 * session owns all storage and may reuse it on the next frame. */
typedef struct {
    void (*packet)(void *context, const H9MainAx25Decoded *packet);
    void (*gps_sentence)(void *context, const uint8_t *sentence,
                         size_t length);
    void (*control)(void *context, H9MainControlAction action);
    void (*version)(void *context, const H9MainAprsVersions *versions);
    void (*serial_write)(void *context, const uint8_t *bytes, size_t length);
} H9MainAprsSessionCallbacks;

typedef struct {
    H9MainUartRing uart_ring;
    H9MainWireReceiver receiver;
    H9MainAprsVersions versions;
    H9MainGpsCollector gps;
    H9MainAx25Decoded decoded;
    H9MainAprsSessionCallbacks callbacks;
    void *callback_context;
} H9MainAprsSession;

void h9_main_aprs_session_init(H9MainAprsSession *session,
                               const H9MainAprsSessionCallbacks *callbacks,
                               void *context);

/* The board UART/DMA producer fills uart_ring and advances write_index.
 * A poll drains at most 1,024 bytes, dispatching each complete frame. No
 * hardware registers, audio gates, or RF transmit path are touched here. */
size_t h9_main_aprs_session_poll(H9MainAprsSession *session);

#endif
