#include "h9_aprs_wire.h"
#include "h9_aprs_tx_permit_protocol.h"

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
        /* Stock keeps the listed packet commands intact. Command 0x53 is
         * reserved for a replacement-to-replacement TX permit. */
        decoder->command = byte == 0x10u || byte == 0x11u ||
                           byte == 0x13u || byte == 0x20u ||
                           byte == 0x21u ||
                           byte == H9_APRS_TX_PERMIT_COMMAND ?
                           byte : (byte & 0x0Fu);
        decoder->has_command = 1;
        return H9_WIRE_MORE;
    }
    if (decoder->command == 1u || decoder->command == 2u ||
        decoder->command == 3u || decoder->command == 4u ||
        decoder->command == 0x0Fu) {
        /* These one-byte settings are raw in stock, even when the byte is
         * 0xDB. Each new byte replaces the previous setting. */
        decoder->payload[0] = byte;
        decoder->length = 1u;
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

typedef struct {
    uint8_t bytes[64];
    size_t used;
    size_t total;
    H9WireWrite write;
    void *context;
} H9WireStream;

static void stream_flush(H9WireStream *stream)
{
    if (stream->used == 0) return;
    stream->write(stream->context, stream->bytes, stream->used);
    stream->used = 0;
}

static void stream_byte(H9WireStream *stream, uint8_t byte)
{
    stream->bytes[stream->used++] = byte;
    stream->total++;
    if (stream->used == sizeof stream->bytes) stream_flush(stream);
}

size_t h9_wire_encode_stream(uint8_t command, const uint8_t *payload,
                             size_t length, H9WireWrite write, void *context)
{
    H9WireStream stream = { { 0 }, 0, 0, write, context };
    size_t index;
    if (write == NULL || (payload == NULL && length != 0) ||
        length > H9_APRS_MAX_SEND) return 0;
    stream_byte(&stream, H9_APRS_FRAME_END);
    stream_byte(&stream, command);
    for (index = 0; index < length; ++index) {
        uint8_t byte = payload[index];
        if (byte == H9_APRS_FRAME_END || byte == H9_APRS_FRAME_ESCAPE) {
            stream_byte(&stream, H9_APRS_FRAME_ESCAPE);
            stream_byte(&stream, byte == H9_APRS_FRAME_END ? 0xDCu : 0xDDu);
        } else {
            stream_byte(&stream, byte);
        }
    }
    stream_byte(&stream, H9_APRS_FRAME_END);
    stream_flush(&stream);
    return stream.total;
}
