#include "h9_aprs_bitstream.h"
#include <stddef.h>
#include <string.h>

void h9_aprs_bitstream_reset(H9AprsBitstream *state)
{
    if (state != NULL) memset(state, 0, sizeof *state);
}

static void abort_frame(H9AprsBitstream *state)
{
    state->in_frame = 0;
    state->preamble_seen = 0;
    state->flag_count = 0;
    state->bit_count = 0;
    state->current_byte = 0;
}

bool h9_aprs_bitstream_feed(H9AprsBitstream *state, uint8_t bit,
                            H9AprsEmitByte emit, void *context)
{
    uint8_t previous_count;
    if (state == NULL || emit == NULL || bit > 1u) return false;
    state->window = (uint8_t)((state->window << 1) | bit);
    if (state->window == 0x7Eu) {
        if (!emit(context, 0x7Eu)) {
            abort_frame(state);
            return false;
        }
        state->in_frame = 1;
        if (state->flag_count < 6u) {
            state->preamble_seen = 0;
            state->flag_count++;
        } else {
            state->preamble_seen = 1;
        }
        state->bit_count = 0;
        state->current_byte = 0;
        return true;
    }
    if ((state->window & 0x7Fu) == 0x7Fu) {
        abort_frame(state);
        return true;
    }
    if (!state->in_frame) {
        state->preamble_seen = 0;
        state->flag_count = 0;
        return true;
    }
    if ((state->window & 0x3Fu) == 0x3Eu) return true; /* stuffed zero */
    if (bit != 0) state->current_byte |= 0x80u;
    previous_count = state->bit_count;
    state->bit_count = (uint8_t)(previous_count + 1u);
    if (state->bit_count < 8u) {
        state->current_byte >>= 1;
        return true;
    }
    if (state->current_byte == 0x7Eu || state->current_byte == 0x7Fu ||
        state->current_byte == 0x1Bu) {
        if (!emit(context, 0x1Bu)) {
            abort_frame(state);
            return false;
        }
    }
    if (!emit(context, state->current_byte)) {
        abort_frame(state);
        return false;
    }
    state->current_byte = 0;
    state->bit_count = 0;
    return true;
}
