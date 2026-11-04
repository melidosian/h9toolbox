#ifndef H9_APRS_FRAME_H
#define H9_APRS_FRAME_H

#include "h9_aprs_wire.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Port of APRS image 0x080051FC. The input is the byte queue emitted by
   0x080055C4: flags and escaped bytes, not raw RF samples. */
typedef struct {
    uint8_t bytes[H9_APRS_MAX_RECEIVE];
    size_t length;
    uint16_t crc;
    uint8_t in_frame;
    uint8_t escaped;
} H9AprsFrame;

typedef void (*H9AprsEmitPacket)(void *context, const uint8_t *data,
                                 size_t length);

void h9_aprs_frame_init(H9AprsFrame *frame);
uint16_t h9_aprs_frame_crc_byte(uint16_t crc, uint8_t byte);
bool h9_aprs_frame_feed(H9AprsFrame *frame, uint8_t byte,
                        H9AprsEmitPacket emit, void *context);

#endif
