#include "h9_aprs_runtime_status.h"

#include <stddef.h>
#include <string.h>

bool h9_aprs_runtime_status_read(const H9AprsRuntimeState *state,
                                 uint8_t output[H9_APRS_STATUS_BYTES])
{
    H9AprsStatusSnapshot snapshot;
    if (state == NULL || output == NULL) return false;
    memset(&snapshot, 0, sizeof snapshot);
    snapshot.words[0] = state->uart1_dma.completed_batches;
    snapshot.words[1] = state->controller.valid_rx_frames;
    snapshot.words[2] = state->controller.forwarded_rx_frames;
    snapshot.words[3] = state->uart1_dma.rejected_batches +
                        state->uart2_rx.overrun_count +
                        state->uart2_rx.dropped_count;
    snapshot.words[4] = state->uart2_board.recovery_failures;
    snapshot.words[5] = state->uart2_board.recovery_successes;
    snapshot.words[6] = state->forwarded_uart2_batches;
    snapshot.words[7] = state->uart1_dma.last_batch_length;
    snapshot.words[8] = state->last_uart1_batch_tick;
    snapshot.words[9] = state->uart1_dma.transfer_errors +
                        state->uart2_dma.transfer_errors +
                        state->uart2_dma.invalid_counts +
                        state->uart1_tx.timeouts;
    snapshot.current_tick = h9_aprs_systick_now(&state->systick);
    snapshot.serial_halfword = state->uart_receive.pending_length;
    snapshot.word_10_low = state->uart_receive.current;
    snapshot.serial_byte_a = state->uart_receive.ready;
    snapshot.serial_byte_b = state->uart_receive.processing;
    h9_aprs_status_encode(&snapshot, output);
    return true;
}
