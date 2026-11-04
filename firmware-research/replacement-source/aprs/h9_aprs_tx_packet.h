#ifndef H9_APRS_TX_PACKET_H
#define H9_APRS_TX_PACKET_H

#include "h9_aprs_tx.h"
#include "h9_aprs_wire.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Port of APRS image 0x0800530C/0x0800534C. The packet storage must remain
   valid until h9_aprs_tx_packet_done() returns true. */
typedef struct {
    const uint8_t *packet;
    size_t length;
    size_t index;
    uint16_t crc;
    uint16_t fcs;
    uint8_t phase;
    uint8_t pending_escape;
    uint8_t pending_byte;
} H9AprsTxPacket;

bool h9_aprs_tx_packet_init(H9AprsTxPacket *encoder,
                            const uint8_t *packet, size_t length);
bool h9_aprs_tx_packet_next(H9AprsTxPacket *encoder, uint8_t *byte);
bool h9_aprs_tx_packet_done(const H9AprsTxPacket *encoder);
size_t h9_aprs_tx_packet_pump(H9AprsTxPacket *encoder, H9AprsTx *tx);

#endif
