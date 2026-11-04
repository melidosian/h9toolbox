#ifndef H9_APRS_TX_GATE_H
#define H9_APRS_TX_GATE_H

#include <stdbool.h>
#include <stdint.h>

/* Cooperative form of the busy/random loop at APRS 0x08005760. */
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
                                        bool channel_busy,
                                        bool receive_active,
                                        bool receive_complete,
                                        uint8_t persistence);

#endif
