#ifndef H9_MAIN_APRS_RECEIVE_H
#define H9_MAIN_APRS_RECEIVE_H

#include <stddef.h>
#include <stdint.h>

/* Stock main app+0x087340 keeps a 1,024-byte receive buffer. */
#define H9_MAIN_APRS_RECEIVE_CAPACITY 1024u

typedef enum {
    H9_MAIN_WIRE_MORE = 0,
    H9_MAIN_WIRE_FRAME = 1,
    H9_MAIN_WIRE_REJECTED = 2
} H9MainWireResult;

typedef struct {
    uint8_t state;
    uint8_t command;
    size_t length;
    uint8_t payload[H9_MAIN_APRS_RECEIVE_CAPACITY];
} H9MainWireReceiver;

void h9_main_wire_reset(H9MainWireReceiver *receiver);
H9MainWireResult h9_main_wire_feed(H9MainWireReceiver *receiver, uint8_t byte);

#endif
