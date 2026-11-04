#ifndef H9_MAIN_APRS_RECEIVE_H
#define H9_MAIN_APRS_RECEIVE_H

#include <stddef.h>
#include <stdint.h>

#include "h9_main_ax25.h"

/* Stock main app+0x087340 keeps a 1,024-byte receive buffer. */
#define H9_MAIN_APRS_RECEIVE_CAPACITY 1024u

typedef enum {
    H9_MAIN_WIRE_MORE = 0,
    H9_MAIN_WIRE_FRAME = 1,
    H9_MAIN_WIRE_REJECTED = 2
} H9MainWireResult;

typedef struct {
    uint8_t state;
    uint8_t frame_ready;
    uint8_t command;
    size_t length;
    uint8_t payload[H9_MAIN_APRS_RECEIVE_CAPACITY];
} H9MainWireReceiver;

void h9_main_wire_reset(H9MainWireReceiver *receiver);
H9MainWireResult h9_main_wire_feed(H9MainWireReceiver *receiver, uint8_t byte);
/* Call after H9_MAIN_WIRE_FRAME. Other stock commands need separate handlers. */
int h9_main_aprs_packet_from_frame(const H9MainWireReceiver *receiver,
                                   H9MainAx25Decoded *decoded);

#endif
