#include "h9_aprs_demod.h"
#include <stddef.h>
#include <string.h>

static int16_t signed_byte(uint8_t value)
{
    return value < 128u ? (int16_t)value : (int16_t)value - 256;
}

static int32_t arithmetic_quarter(int32_t value)
{
    return value >= 0 ? value / 4 : -((-value + 3) / 4);
}

static int16_t arithmetic_half(int16_t value)
{
    return value >= 0 ? (int16_t)(value / 2) : (int16_t)-((-value + 1) / 2);
}

static bool emit_received_byte(void *context, uint8_t byte)
{
    H9AprsDemod *demod = (H9AprsDemod *)context;
    return h9_aprs_ring_push(&demod->receive, byte);
}

void h9_aprs_demod_init(H9AprsDemod *demod)
{
    unsigned index;
    if (demod == NULL) return;
    memset(demod, 0, sizeof *demod);
    (void)h9_aprs_ring_init(&demod->delay, demod->delay_bytes,
                            sizeof demod->delay_bytes);
    (void)h9_aprs_ring_init(&demod->receive, demod->receive_bytes,
                            sizeof demod->receive_bytes);
    for (index = 0; index < 4; ++index)
        (void)h9_aprs_ring_push(&demod->delay, 0);
    atomic_init(&demod->queue_overflow, 0u);
    h9_aprs_bitstream_reset(&demod->bitstream);
}

bool h9_aprs_demod_sample(H9AprsDemod *demod, int16_t normalized_sample)
{
    uint8_t delayed_byte;
    uint8_t recent;
    uint8_t bit;
    int32_t filter;
    if (demod == NULL || normalized_sample < -128 ||
        normalized_sample > 127) return false;
    if (!h9_aprs_ring_pop(&demod->delay, &delayed_byte)) return false;
    if (!h9_aprs_ring_push(&demod->delay, (uint8_t)normalized_sample))
        return false;
    demod->correlation_previous = demod->correlation_current;
    demod->correlation_current = (int16_t)arithmetic_quarter(
        (int32_t)normalized_sample * signed_byte(delayed_byte));
    demod->filter_previous = demod->filter_current;
    filter = (int32_t)demod->correlation_previous + demod->correlation_current +
        arithmetic_half(demod->filter_previous);
    demod->filter_current = (int16_t)filter;
    demod->symbol_history = (uint8_t)((demod->symbol_history << 1) |
                                       (demod->filter_current < 1 ? 1u : 0u));
    if (((demod->symbol_history ^ (demod->symbol_history >> 2)) & 3u) == 3u) {
        demod->timing += demod->timing < 32 ? 1 : -1;
        demod->no_transition_count = 0;
    } else {
        demod->no_transition_count++;
    }
    demod->timing += 8;
    if (demod->timing > 63) {
        demod->timing -= 64;
        recent = (uint8_t)(demod->symbol_history & 7u);
        demod->bit_history = (uint8_t)((demod->bit_history << 1) |
            (recent == 7u || recent == 6u || recent == 5u || recent == 3u));
        bit = (uint8_t)((((demod->bit_history ^
                           (demod->bit_history >> 1)) & 1u) ^ 1u));
        if (!h9_aprs_bitstream_feed(&demod->bitstream, bit,
                                    emit_received_byte, demod)) {
            uint32_t previous = atomic_load_explicit(&demod->queue_overflow,
                                                       memory_order_relaxed);
            atomic_store_explicit(&demod->queue_overflow, previous + 1u,
                                  memory_order_release);
        }
    }
    if (demod->no_transition_count > 0x60u) {
        demod->no_transition_count = 0;
        demod->bitstream.preamble_seen = 0;
    }
    return true;
}

bool h9_aprs_demod_pop_byte(H9AprsDemod *demod, uint8_t *byte)
{
    return demod != NULL && h9_aprs_ring_pop(&demod->receive, byte);
}
