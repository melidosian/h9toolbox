#ifndef H9_APRS_BITSTREAM_H
#define H9_APRS_BITSTREAM_H

#include <stdbool.h>
#include <stdint.h>

/* Port of the recovered-bit path at APRS image 0x080055C4. The caller at
   0x08002308 supplies NRZI-decoded bits; this layer does not demodulate ADC
   samples or validate an AX.25 FCS. */
typedef struct {
    uint8_t window;
    uint8_t bit_count;
    uint8_t current_byte;
    uint8_t in_frame;
    uint8_t preamble_seen;
    uint8_t flag_count;
} H9AprsBitstream;

typedef bool (*H9AprsEmitByte)(void *context, uint8_t byte);

void h9_aprs_bitstream_reset(H9AprsBitstream *state);
bool h9_aprs_bitstream_feed(H9AprsBitstream *state, uint8_t bit,
                            H9AprsEmitByte emit, void *context);

#endif
