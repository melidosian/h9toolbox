#ifndef H9_MAIN_AX25_H
#define H9_MAIN_AX25_H

#include <stddef.h>
#include <stdint.h>

#define H9_MAIN_AX25_MAX_DIGI 6u
#define H9_MAIN_AX25_MAX_PACKET 512u

/* Stock app+0x05B24A/0x05B298 uses six ASCII callsign bytes and one raw
   SSID/flags byte per address. Zero callsign bytes become spaces on air. */
typedef struct {
    uint8_t destination[7];
    uint8_t source[7];
    uint8_t digipeaters[H9_MAIN_AX25_MAX_DIGI][7];
    uint8_t digipeater_count;
    uint8_t pid; /* Zero requests the stock default, 0xF0. */
    const uint8_t *information;
    size_t information_length;
} H9MainAx25Packet;

size_t h9_main_ax25_encode(const H9MainAx25Packet *packet,
                            uint8_t *output, size_t capacity);

#endif
