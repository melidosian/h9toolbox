#ifndef H9_APRS_TX_GATE_H
#define H9_APRS_TX_GATE_H

#include <stdbool.h>
#include <stdint.h>

/* Cooperative gate based on stock 0x08005760. Its inputs correspond to
 * demodulator state at TX object 0x2000014C: +4 is the six-flag preamble,
 * +3 is in-frame, and +0xE0 records a receive queue error. They are not
 * carrier-sense GPIO signals. */
typedef struct {
    uint32_t random_state;
} H9AprsTxGate;

typedef enum {
    H9_APRS_TX_WAIT,
    H9_APRS_TX_SEND,
    H9_APRS_TX_DROP
} H9AprsTxGateResult;

void h9_aprs_tx_gate_init(H9AprsTxGate *gate, uint32_t random_seed);
H9AprsTxGateResult h9_aprs_tx_gate_step(H9AprsTxGate *gate,
                                        bool preamble_seen,
                                        bool in_frame,
                                        bool queue_error,
                                        uint8_t persistence);

#endif
