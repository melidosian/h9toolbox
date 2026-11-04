#ifndef H9_MAIN_APRS_WIRE_H
#define H9_MAIN_APRS_WIRE_H

#include <stddef.h>
#include <stdint.h>

/* Main app+0x05B32E uses five leading and three trailing delimiters.
   Its observed caller supplies a 2,048-byte output buffer. */
#define H9_MAIN_APRS_WIRE_CAPACITY 2048u
#define H9_MAIN_APRS_MAX_PAYLOAD ((H9_MAIN_APRS_WIRE_CAPACITY - 10u) / 2u)

size_t h9_main_aprs_wire_encode(uint8_t command, const uint8_t *payload,
                                 size_t length, uint8_t *output,
                                 size_t capacity);

#endif
