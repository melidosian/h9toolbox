#include "h9_aprs_wire.h"

void h9_wire_reset(H9WireDecoder *decoder)
{
    if (decoder == NULL) return;
    decoder->started = 0;
    decoder->escaped = 0;
    decoder->has_command = 0;
    decoder->command = 0;
    decoder->length = 0;
}

H9WireResult h9_wire_feed(H9WireDecoder *decoder, uint8_t byte)
{
    if (decoder == NULL) return H9_WIRE_REJECTED;
    if (byte == H9_APRS_FRAME_END) {
        if (!decoder->started) {
            decoder->escaped = 0;
            decoder->has_command = 0;
            decoder->length = 0;
            decoder->started = 1;
            return H9_WIRE_MORE;
        }
        if (decoder->escaped) {
            h9_wire_reset(decoder);
            decoder->started = 1;
            return H9_WIRE_REJECTED;
        }
        if (decoder->has_command) {
            decoder->started = 0;
            return H9_WIRE_FRAME;
        }
        decoder->started = 1;
        return H9_WIRE_MORE;
    }
    if (!decoder->started) return H9_WIRE_MORE;
    if (!decoder->has_command) {
        decoder->command = byte;
        decoder->has_command = 1;
        return H9_WIRE_MORE;
    }
    if (decoder->escaped) {
        decoder->escaped = 0;
        if (byte == 0xDCu) byte = H9_APRS_FRAME_END;
        else if (byte == 0xDDu) byte = H9_APRS_FRAME_ESCAPE;
        else {
            h9_wire_reset(decoder);
            return H9_WIRE_REJECTED;
        }
    } else if (byte == H9_APRS_FRAME_ESCAPE) {
        decoder->escaped = 1;
        return H9_WIRE_MORE;
    }
    if (decoder->length == H9_APRS_MAX_RECEIVE) {
        h9_wire_reset(decoder);
        return H9_WIRE_REJECTED;
    }
    decoder->payload[decoder->length++] = byte;
    return H9_WIRE_MORE;
}

size_t h9_wire_encode(uint8_t command, const uint8_t *payload, size_t length,
                      uint8_t *output, size_t capacity)
{
    size_t index;
    size_t used = 0;
    if (output == NULL || (length != 0 && payload == NULL) ||
        length > H9_APRS_MAX_SEND || capacity < 3u + 2u * length) return 0;
    output[used++] = H9_APRS_FRAME_END;
    output[used++] = command;
    for (index = 0; index < length; ++index) {
        uint8_t byte = payload[index];
        if (byte == H9_APRS_FRAME_END || byte == H9_APRS_FRAME_ESCAPE) {
            output[used++] = H9_APRS_FRAME_ESCAPE;
            output[used++] = byte == H9_APRS_FRAME_END ? 0xDCu : 0xDDu;
        } else {
            output[used++] = byte;
        }
    }
    output[used++] = H9_APRS_FRAME_END;
    return used;
}
