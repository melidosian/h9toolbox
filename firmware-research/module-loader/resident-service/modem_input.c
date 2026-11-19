#include "modem_input.h"

void h9_modem_poll(struct h9_modem_cursor *cursor, struct h9_bs *beacon,
                   const volatile uint8_t *buffer,
                   const volatile uint32_t *produced, uint32_t now_ms)
{
    uint8_t batch[H9_MODEM_POLL_BYTES];
    if (!buffer) {
        if (cursor->attached) h9_bs_input_gap(beacon, now_ms);
        cursor->attached = 0;
        return;
    }
    uint32_t head = *produced;
    if (!cursor->attached) {
        cursor->next = head;
        cursor->attached = 1;
        return;
    }
    const unsigned reliable = H9_MODEM_DMA_CAPACITY - H9_MODEM_DMA_CHUNK;
    uint32_t available = head - cursor->next;
    if (!available) return;
    if (available <= reliable) {
        unsigned count = available > sizeof batch ? sizeof batch : (unsigned)available;
        for (unsigned i = 0; i < count; ++i)
            batch[i] = buffer[(cursor->next + i) & (H9_MODEM_DMA_CAPACITY - 1u)];
        head = *produced;
        if (head - cursor->next <= reliable) {
            cursor->next += count;
            for (unsigned i = 0; i < count; ++i) h9_bs_feed_byte(beacon, batch[i], now_ms);
            return;
        }
    }
    cursor->next = head;
    h9_bs_input_gap(beacon, now_ms);
}
