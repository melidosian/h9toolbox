#ifndef H9_MAIN_TX_SERVICE_H
#define H9_MAIN_TX_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "h9_main_aprs.h"

typedef enum {
    H9_MAIN_TX_REJECTED,
    H9_MAIN_TX_PREPARE_FAILED,
    H9_MAIN_TX_READBACK_FAILED,
    H9_MAIN_TX_SEND_FAILED,
    H9_MAIN_TX_CLEANUP_FAILED,
    H9_MAIN_TX_SENT
} H9MainTxResult;

/* A board adapter must leave PTT inactive throughout prepare_tx and
 * read_prepared_tx. send_packet is the only callback allowed to key RF.
 * stop_tx must deassert PTT even when send_packet reports failure. */
typedef struct {
    bool (*read_current_side)(void *opaque, H9Side *side);
    bool (*prepare_tx)(void *opaque, H9Side side, uint32_t tx_hz);
    bool (*read_prepared_tx)(void *opaque, H9Side *side, uint32_t *tx_hz);
    bool (*send_packet)(void *opaque, const uint8_t *packet, size_t length);
    bool (*stop_tx)(void *opaque);
    bool (*restore_receive)(void *opaque, H9Side previous_side);
} H9MainTxHardware;

/* The packet is an already assembled AX.25 frame. This transaction rejects
 * Lock requests on the wrong active side and verifies RF configuration
 * before any callback may key. It always requests TX stop and RX restore
 * after a prepare attempt, including failed preparation or readback. */
H9MainTxResult h9_main_tx_send(const H9MainAprsConfig *config,
                               uint32_t a_tx_hz, uint32_t b_tx_hz,
                               const uint8_t *packet, size_t length,
                               const H9MainTxHardware *hardware, void *opaque);

#endif
