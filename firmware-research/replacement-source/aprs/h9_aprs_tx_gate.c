#include "h9_aprs_tx_gate.h"
#include <stddef.h>

void h9_aprs_tx_gate_init(H9AprsTxGate *gate, uint32_t random_seed)
{
    if (gate != NULL) gate->random_state = random_seed;
}

H9AprsTxGateResult h9_aprs_tx_gate_step(H9AprsTxGate *gate,
                                        bool channel_busy,
                                        bool receive_active,
                                        bool receive_complete,
                                        uint8_t persistence)
{
    uint8_t draw;
    if (gate == NULL) return H9_APRS_TX_WAIT;
    if (channel_busy) {
        /* Stock 0x08005760 processes RX frames until an RX event ends TX. */
        if (receive_active && receive_complete) return H9_APRS_TX_DROP;
        return H9_APRS_TX_WAIT;
    }
    /* Stock 0x0800211C: rand_state = rand_state * 1103515245 + 12345.
       0x08005760 compares the low byte of rand_state >> 1 to persistence. */
    gate->random_state = gate->random_state * 0x41C64E6Du + 0x3039u;
    draw = (uint8_t)(gate->random_state >> 1);
    return draw < persistence ? H9_APRS_TX_SEND : H9_APRS_TX_WAIT;
}
