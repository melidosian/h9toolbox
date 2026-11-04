#include "h9_main_aprs_receive.h"

enum {
    H9_WIRE_IDLE = 0,
    H9_WIRE_COMMAND = 1,
    H9_WIRE_PAYLOAD = 2,
    H9_WIRE_ESCAPE = 3
};

void h9_main_wire_reset(H9MainWireReceiver *receiver)
{
    if (receiver == NULL) return;
    receiver->state = H9_WIRE_IDLE;
    receiver->command = 0;
    receiver->length = 0;
}

static H9MainWireResult append_byte(H9MainWireReceiver *receiver, uint8_t byte)
{
    receiver->state = H9_WIRE_PAYLOAD;
    if (receiver->length == H9_MAIN_APRS_RECEIVE_CAPACITY) {
        h9_main_wire_reset(receiver);
        return H9_MAIN_WIRE_REJECTED;
    }
    receiver->payload[receiver->length++] = byte;
    return H9_MAIN_WIRE_MORE;
}

H9MainWireResult h9_main_wire_feed(H9MainWireReceiver *receiver, uint8_t byte)
{
    if (receiver == NULL) return H9_MAIN_WIRE_REJECTED;
    switch (receiver->state) {
    case H9_WIRE_IDLE:
        if (byte == 0xC0u) {
            receiver->length = 0;
            receiver->state = H9_WIRE_COMMAND;
        }
        return H9_MAIN_WIRE_MORE;
    case H9_WIRE_COMMAND:
        if (byte != 0xC0u) {
            receiver->command = byte;
            receiver->state = H9_WIRE_PAYLOAD;
        }
        return H9_MAIN_WIRE_MORE;
    case H9_WIRE_PAYLOAD:
        if (byte == 0xC0u) {
            receiver->state = H9_WIRE_IDLE;
            return H9_MAIN_WIRE_FRAME;
        }
        if (byte == 0xDBu) {
            receiver->state = H9_WIRE_ESCAPE;
            return H9_MAIN_WIRE_MORE;
        }
        return append_byte(receiver, byte);
    case H9_WIRE_ESCAPE:
        if (byte == 0xDCu) byte = 0xC0u;
        else if (byte == 0xDDu) byte = 0xDBu;
        /* The stock handler accepts other escaped bytes literally. */
        return append_byte(receiver, byte);
    default:
        h9_main_wire_reset(receiver);
        return H9_MAIN_WIRE_REJECTED;
    }
}
