#ifndef H9_MAIN_APRS_CONTROL_H
#define H9_MAIN_APRS_CONTROL_H

#include <stddef.h>
#include <stdint.h>

#include "h9_main_aprs_receive.h"

/* Raw group/opcode pairs routed by stock main app+0x0870BC. */
typedef enum {
    H9_MAIN_CONTROL_NONE = 0,
    H9_MAIN_CONTROL_06_01 = 0x0601,
    H9_MAIN_CONTROL_06_02 = 0x0602,
    H9_MAIN_CONTROL_06_03 = 0x0603,
    H9_MAIN_CONTROL_06_04 = 0x0604,
    H9_MAIN_CONTROL_06_05 = 0x0605,
    H9_MAIN_CONTROL_06_06 = 0x0606,
    H9_MAIN_CONTROL_0A_01 = 0x0A01,
    H9_MAIN_CONTROL_0A_03 = 0x0A03
} H9MainControlAction;

/* Stock CRC: initial 0xFFFF, polynomial 0x1021, no reflection or final XOR. */
uint16_t h9_main_control_crc(const uint8_t *data, size_t length);

/* Returns 1 only for a complete command-0x51 frame with valid CRC and a
 * recognized group/opcode. No stock side effect is performed here. */
int h9_main_control_from_frame(const H9MainWireReceiver *receiver,
                               H9MainControlAction *action);

#endif
