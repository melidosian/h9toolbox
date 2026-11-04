#include "h9_aprs_tx.h"
#include <stddef.h>
#include <string.h>

static uint16_t flag_count(uint16_t milliseconds)
{
    /* FUN_08002664: (setting * 1200 + 4000) / 8000, rounded. */
    return (uint16_t)(((uint32_t)milliseconds * 1200u + 4000u) / 8000u);
}

static void stop_tx(H9AprsTx *tx)
{
    tx->active = 0;
    tx->lead_flags = 0;
    tx->tail_flags = 0;
}

static void toggle_tone(H9AprsTx *tx)
{
    uint16_t next = tx->oscillator.step == 0x40u ? 0x75u : 0x40u;
    (void)h9_aprs_oscillator_set_step(&tx->oscillator, next);
}

void h9_aprs_tx_init(H9AprsTx *tx, uint16_t lead_ms, uint16_t tail_ms)
{
    if (tx == NULL) return;
    memset(tx, 0, sizeof *tx);
    (void)h9_aprs_ring_init(&tx->queue, tx->queue_bytes,
                            sizeof tx->queue_bytes);
    h9_aprs_oscillator_init(&tx->oscillator);
    tx->lead_ms = lead_ms;
    tx->tail_ms = tail_ms;
}

bool h9_aprs_tx_enqueue(H9AprsTx *tx, uint8_t byte)
{
    if (tx == NULL || h9_aprs_ring_full(&tx->queue)) return false;
    if (!tx->active) {
        h9_aprs_oscillator_init(&tx->oscillator);
        tx->one_count = 0;
        tx->active = 1;
        tx->frame_data = 0;
        tx->bit_mask = 0;
        tx->samples_left = 0;
        tx->lead_flags = flag_count(tx->lead_ms);
    }
    tx->tail_flags = flag_count(tx->tail_ms);
    return h9_aprs_ring_push(&tx->queue, byte);
}

bool h9_aprs_tx_next_sample(H9AprsTx *tx, uint8_t *sample)
{
    uint8_t next;
    if (tx == NULL || sample == NULL || !tx->active) return false;
    if (tx->samples_left == 0) {
        if (tx->bit_mask == 0) {
            if (h9_aprs_ring_empty(&tx->queue) && tx->tail_flags == 0) {
                stop_tx(tx);
                return false;
            }
            if (!tx->frame_data) tx->one_count = 0;
            tx->frame_data = 1;
            if (tx->lead_flags != 0) {
                tx->lead_flags--;
                tx->current_byte = 0x7Eu;
            } else if (!h9_aprs_ring_empty(&tx->queue)) {
                (void)h9_aprs_ring_pop(&tx->queue, &tx->current_byte);
            } else {
                tx->tail_flags--;
                tx->current_byte = 0x7Eu;
            }
            if (tx->current_byte == 0x1Bu) {
                if (!h9_aprs_ring_pop(&tx->queue, &next)) {
                    stop_tx(tx);
                    return false;
                }
                tx->current_byte = next;
            } else if (tx->current_byte == 0x7Eu ||
                       tx->current_byte == 0x7Fu) {
                tx->frame_data = 0;
            }
            tx->bit_mask = 1;
        }
        if (tx->frame_data && tx->one_count >= 5u) {
            tx->one_count = 0;
            toggle_tone(tx); /* stuffed zero; retry same data bit next */
        } else {
            if ((tx->current_byte & tx->bit_mask) != 0)
                tx->one_count++;
            else {
                tx->one_count = 0;
                toggle_tone(tx); /* NRZI zero */
            }
            tx->bit_mask = (uint8_t)(tx->bit_mask << 1);
        }
        tx->samples_left = 8;
    }
    *sample = h9_aprs_oscillator_next(&tx->oscillator);
    tx->samples_left--;
    return true;
}
