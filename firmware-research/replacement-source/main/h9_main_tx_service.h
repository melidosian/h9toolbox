#ifndef H9_MAIN_TX_SERVICE_H
#define H9_MAIN_TX_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "h9_main_aprs.h"
#include "h9_main_beacon.h"
#include "h9_main_rf_state.h"

typedef enum {
    H9_MAIN_TX_REJECTED,
    H9_MAIN_TX_ROUTE_BUSY,
    H9_MAIN_TX_PREPARE_FAILED,
    H9_MAIN_TX_READBACK_FAILED,
    H9_MAIN_TX_ROUTE_FAILED,
    H9_MAIN_TX_SEND_FAILED,
    H9_MAIN_TX_CLEANUP_FAILED,
    H9_MAIN_TX_SENT
} H9MainTxResult;

/* read_current_side must report bit 7 of the working channel-state byte
 * (stock RAM 0x11B50), which governs the Lock choice; it must not infer side
 * from the highlighted display line. With Dual Watch, RF setup separately
 * reads the routed side byte at 0x11AEC. A board adapter must aim both
 * selectors at the planned side before RF setup and restore them afterward;
 * h9_main_rf_route_prepare/restore implements the selector writes. The
 * begin_route/end_route must reserve the side and channel state against
 * Dual Watch or menu updates for the entire transaction, including cleanup.
 * read_position_status captures the GPS-source, fix-valid, and coordinate
 * fields under the same reservation before RF preparation.
 * This is a state reservation, not a request to mask interrupts for the
 * packet duration. A failed begin_route must not retain the reservation.
 * end_route returns false if release or its final state check fails.
 * The adapter must check current band/region settings in validate_frequency
 * and leave PTT inactive throughout prepare_tx, read_prepared_tx, and
 * read_rf_snapshot. The snapshot captures the live Dual Watch route and
 * working frequency pair after preparation. send_packet is the only callback
 * allowed to key RF and must return only when this packet has finished or
 * failed; the route remains reserved until stop_tx and restore_receive run.
 * stop_tx must deassert PTT even when send_packet reports failure. */
typedef struct {
    bool (*begin_route)(void *opaque);
    bool (*end_route)(void *opaque);
    bool (*read_current_side)(void *opaque, H9Side *side);
    bool (*read_position_status)(void *opaque, H9BeaconPositionStatus *status);
    bool (*validate_frequency)(void *opaque, uint32_t tx_hz);
    /* Receives the complete plan so a board adapter can distinguish an
     * Auto side switch from a Lock/Current guard during route preparation. */
    bool (*prepare_tx)(void *opaque, const H9TxPlan *plan);
    bool (*read_prepared_tx)(void *opaque, H9Side *side, uint32_t *tx_hz);
    bool (*read_rf_snapshot)(void *opaque, H9MainRfSnapshot *snapshot);
    bool (*send_packet)(void *opaque, const uint8_t *packet, size_t length);
    bool (*stop_tx)(void *opaque);
    bool (*restore_receive)(void *opaque, H9Side previous_side);
} H9MainTxHardware;

/* The packet is an already assembled AX.25 frame. This transaction rejects
 * a missing required GPS position and Lock requests on the wrong active
 * side, requires a frequency precheck,
 * and verifies RF configuration before any callback may key. It always
 * requests TX stop and RX restore after a prepare attempt, including failed
 * preparation or readback. */
H9MainTxResult h9_main_tx_send(const H9MainAprsConfig *config,
                               uint32_t a_tx_hz, uint32_t b_tx_hz,
                               const uint8_t *packet, size_t length,
                               const H9MainTxHardware *hardware, void *opaque);

#endif
