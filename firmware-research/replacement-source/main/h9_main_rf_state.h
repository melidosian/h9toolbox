#ifndef H9_MAIN_RF_STATE_H
#define H9_MAIN_RF_STATE_H

#include "h9_main_aprs.h"
#include <stdbool.h>
#include <stdint.h>

/* Stock main RAM fields read by the APRS TX and RF-side setup paths. */
typedef struct {
    const volatile uint8_t *dual_watch;
    const volatile uint8_t *route_side;
    const volatile uint8_t *channel_side_state;
    const volatile uint32_t *working_rx_10hz;
    const volatile uint32_t *working_tx_10hz;
    const volatile uint32_t *a_rx_10hz;
    const volatile uint32_t *a_tx_10hz;
    const volatile uint32_t *b_rx_10hz;
    const volatile uint32_t *b_tx_10hz;
} H9MainRfStateMemory;

typedef struct {
    bool dual_watch;
    H9Side logical_side;
    H9Side setup_side;
    H9MainRfChannelFrequencies working;
    H9MainRfChannelFrequencies a;
    H9MainRfChannelFrequencies b;
} H9MainRfSnapshot;

/* Binding reads no memory and changes no RF output. */
void h9_main_rf_state_bind_target(H9MainRfStateMemory *memory);

/* Reads selector bytes again after the frequency words; an observed change
 * rejects the snapshot. A live TX adapter must also serialize RF state
 * against Dual Watch updates until the send decision is complete. */
bool h9_main_rf_state_read(const H9MainRfStateMemory *memory,
                           H9MainRfSnapshot *snapshot);

/* This verifies side routing and the copied working frequency pair only.
 * It cannot verify the final synthesizer frequency or emitted RF. */
bool h9_main_rf_snapshot_matches_plan(const H9MainRfSnapshot *snapshot,
                                      const H9MainAprsConfig *config,
                                      const H9TxPlan *plan);

#endif
