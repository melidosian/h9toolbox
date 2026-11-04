#include "h9_aprs_tx_packet.h"
#include "h9_aprs_frame.h"
#include <stddef.h>
#include <string.h>

enum {
    TX_PACKET_START = 1,
    TX_PACKET_PAYLOAD,
    TX_PACKET_FCS_LOW,
    TX_PACKET_FCS_HIGH,
    TX_PACKET_END,
    TX_PACKET_DONE
};

static void escape_byte(H9AprsTxPacket *encoder, uint8_t value,
                        uint8_t *output)
{
    if (value == 0x7Eu || value == 0x7Fu || value == 0x1Bu) {
        encoder->pending_escape = 1;
        encoder->pending_byte = value;
        *output = 0x1Bu;
    } else {
        *output = value;
    }
}

bool h9_aprs_tx_packet_init(H9AprsTxPacket *encoder,
                            const uint8_t *packet, size_t length)
{
    if (encoder == NULL || packet == NULL || length == 0 ||
        length > H9_APRS_MAX_RECEIVE) return false;
    memset(encoder, 0, sizeof *encoder);
    encoder->packet = packet;
    encoder->length = length;
    encoder->crc = 0xFFFFu;
    encoder->phase = TX_PACKET_START;
    return true;
}

bool h9_aprs_tx_packet_next(H9AprsTxPacket *encoder, uint8_t *byte)
{
    uint8_t value;
    if (encoder == NULL || byte == NULL || encoder->phase == 0 ||
        encoder->phase == TX_PACKET_DONE) return false;
    if (encoder->pending_escape) {
        encoder->pending_escape = 0;
        *byte = encoder->pending_byte;
        return true;
    }
    for (;;) {
        switch (encoder->phase) {
        case TX_PACKET_START:
            encoder->phase = TX_PACKET_PAYLOAD;
            *byte = 0x7Eu;
            return true;
        case TX_PACKET_PAYLOAD:
            if (encoder->index == encoder->length) {
                encoder->fcs = (uint16_t)~encoder->crc;
                encoder->phase = TX_PACKET_FCS_LOW;
                break;
            }
            value = encoder->packet[encoder->index++];
            encoder->crc = h9_aprs_frame_crc_byte(encoder->crc, value);
            escape_byte(encoder, value, byte);
            return true;
        case TX_PACKET_FCS_LOW:
            value = (uint8_t)encoder->fcs;
            encoder->crc = h9_aprs_frame_crc_byte(encoder->crc, value);
            encoder->phase = TX_PACKET_FCS_HIGH;
            escape_byte(encoder, value, byte);
            return true;
        case TX_PACKET_FCS_HIGH:
            value = (uint8_t)(encoder->fcs >> 8);
            encoder->crc = h9_aprs_frame_crc_byte(encoder->crc, value);
            encoder->phase = TX_PACKET_END;
            escape_byte(encoder, value, byte);
            return true;
        case TX_PACKET_END:
            encoder->phase = TX_PACKET_DONE;
            *byte = 0x7Eu;
            return true;
        default:
            return false;
        }
    }
}

bool h9_aprs_tx_packet_done(const H9AprsTxPacket *encoder)
{
    return encoder != NULL && encoder->phase == TX_PACKET_DONE &&
        !encoder->pending_escape;
}

size_t h9_aprs_tx_packet_pump(H9AprsTxPacket *encoder, H9AprsTx *tx)
{
    uint8_t byte;
    size_t count = 0;
    if (encoder == NULL || tx == NULL || tx->queue.capacity < 2u) return 0;
    while (!h9_aprs_ring_full(&tx->queue)) {
        H9AprsTxPacket next = *encoder;
        size_t free_slots = (tx->queue.head + tx->queue.capacity -
                             tx->queue.tail - 1u) % tx->queue.capacity;
        if (!h9_aprs_tx_packet_next(&next, &byte)) break;
        /* Keep an escape marker and its literal together in the queue. */
        if (next.pending_escape && free_slots < 2u) break;
        if (!h9_aprs_tx_enqueue(tx, byte)) break;
        *encoder = next;
        count++;
    }
    return count;
}
