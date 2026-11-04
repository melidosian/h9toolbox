#ifndef H9_MAIN_TX_ADAPTER_H
#define H9_MAIN_TX_ADAPTER_H

#include "h9_main_aprs_uart1_link.h"
#include "h9_main_tx_service.h"
#include "h9_main_tx_ram_policy.h"

/* The board must supply these operations from verified hardware paths.
 * prepare_unkeyed must leave RF off; read_synth must inspect the programmed
 * synthesizer, not only the copied channel RAM. unkey_rf must confirm that
 * PTT is deasserted before returning true. The route reservation must
 * exclude Dual Watch and menu changes without masking interrupts for TX. */
typedef struct {
    bool (*route_acquire)(void *context);
    bool (*route_release)(void *context);
    bool (*prepare_unkeyed)(void *context, const H9TxPlan *plan);
    bool (*read_synth)(void *context, H9Side *side, uint32_t *tx_hz);
    bool (*key_rf)(void *context);
    bool (*unkey_rf)(void *context);
    bool (*restore_rf_receive)(void *context, H9Side previous_side);
    uint32_t (*now_ms)(void *context);
    bool (*yield)(void *context);
    void *context;
} H9MainTxBoardHooks;

typedef struct {
    H9MainAprsUart1Link *uart;
    H9MainRfStateMemory route;
    H9MainTxRamPolicy policy;
    H9MainRfRouteSaved saved_route;
    H9MainTxBoardHooks board;
    uint32_t timeout_ms;
    uint32_t max_poll_steps;
    bool reserved;
    bool stop_confirmed;
    bool receive_restored;
} H9MainTxAdapter;

/* This completes the source-level route/UART transaction. It remains
 * diagnostic until every board hook is mapped and bench verified. */
H9MainTxResult h9_main_tx_adapter_send(H9MainTxAdapter *adapter,
                                        const H9MainAprsConfig *config,
                                        uint32_t a_tx_hz, uint32_t b_tx_hz,
                                        const uint8_t *packet, size_t length);

#endif
