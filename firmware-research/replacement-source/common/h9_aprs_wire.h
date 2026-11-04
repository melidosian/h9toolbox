#ifndef H9_APRS_WIRE_H
#define H9_APRS_WIRE_H

#include <stddef.h>
#include <stdint.h>

/* Observed at APRS image 0x08004894 and 0x08005B8C. */
#define H9_APRS_FRAME_END 0xC0u
#define H9_APRS_FRAME_ESCAPE 0xDBu
#define H9_APRS_MAX_RECEIVE 0x318u
#define H9_APRS_MAX_SEND 0x3FCu
#define H9_APRS_ENCODED_CAPACITY (3u + 2u * H9_APRS_MAX_SEND)

typedef enum {
    H9_WIRE_MORE = 0,
    H9_WIRE_FRAME = 1,
    H9_WIRE_REJECTED = 2
} H9WireResult;

typedef struct {
    uint8_t started;
    uint8_t escaped;
    uint8_t has_command;
    uint8_t command;
    size_t length;
    uint8_t payload[H9_APRS_MAX_RECEIVE];
} H9WireDecoder;

void h9_wire_reset(H9WireDecoder *decoder);
H9WireResult h9_wire_feed(H9WireDecoder *decoder, uint8_t byte);
size_t h9_wire_encode(uint8_t command, const uint8_t *payload, size_t length,
                      uint8_t *output, size_t capacity);

#endif
