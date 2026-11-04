#include "h9_aprs_frame.h"
#include <stddef.h>
#include <string.h>

uint16_t h9_aprs_frame_crc_byte(uint16_t crc, uint8_t byte)
{
    unsigned bit;
    crc ^= byte;
    for (bit = 0; bit < 8; ++bit)
        crc = (uint16_t)((crc >> 1) ^ ((crc & 1u) ? 0x8408u : 0u));
    return crc;
}

void h9_aprs_frame_init(H9AprsFrame *frame)
{
    if (frame == NULL) return;
    memset(frame, 0, sizeof *frame);
    frame->crc = 0xFFFFu;
}

bool h9_aprs_frame_feed(H9AprsFrame *frame, uint8_t byte,
                        H9AprsEmitPacket emit, void *context)
{
    bool accepted = false;
    if (frame == NULL) return false;
    if (!frame->escaped && byte == 0x7Eu) {
        if (frame->length > 0x11u && frame->crc == 0xF0B8u &&
            emit != NULL) {
            emit(context, frame->bytes, frame->length - 2u);
            accepted = true;
        }
        frame->in_frame = 1;
        frame->crc = 0xFFFFu;
        frame->length = 0;
    } else if (!frame->escaped && byte == 0x7Fu) {
        frame->in_frame = 0;
    } else if (!frame->escaped && byte == 0x1Bu) {
        frame->escaped = 1;
    } else {
        if (frame->in_frame) {
            if (frame->length < sizeof frame->bytes) {
                frame->bytes[frame->length++] = byte;
                frame->crc = h9_aprs_frame_crc_byte(frame->crc, byte);
            } else {
                frame->in_frame = 0;
            }
        }
        frame->escaped = 0;
    }
    return accepted;
}
