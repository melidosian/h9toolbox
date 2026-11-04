#ifndef H9_APRS_TX_H
#define H9_APRS_TX_H

#include "h9_aprs_oscillator.h"
#include "h9_aprs_ring.h"
#include <stdbool.h>
#include <stdint.h>

/* Queue and waveform scheduler from APRS image 0x08002664 and 0x08002468.
   Input is the stock-style encoded byte stream with 0x1B escapes. */
typedef struct {
    uint8_t queue_bytes[64];
    H9AprsRing queue;
    H9AprsOscillator oscillator;
    uint16_t lead_ms;
    uint16_t tail_ms;
    uint16_t lead_flags;
    uint16_t tail_flags;
    uint8_t active;
    uint8_t current_byte;
    uint8_t bit_mask;
    uint8_t frame_data;
    uint8_t one_count;
    uint8_t samples_left;
} H9AprsTx;

void h9_aprs_tx_init(H9AprsTx *tx, uint16_t lead_ms, uint16_t tail_ms);
bool h9_aprs_tx_enqueue(H9AprsTx *tx, uint8_t byte);
bool h9_aprs_tx_next_sample(H9AprsTx *tx, uint8_t *sample);

#endif
