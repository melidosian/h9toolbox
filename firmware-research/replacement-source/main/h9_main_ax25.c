#include "h9_main_ax25.h"

static size_t encode_address(uint8_t *output, size_t offset,
                             const uint8_t address[7], int last)
{
    size_t index;
    uint8_t flags;
    for (index = 0; index < 6u; ++index)
        output[offset + index] = address[index] == 0 ? 0x40u :
            (uint8_t)(address[index] << 1);
    flags = (uint8_t)((address[6] & 0x80u) |
                      ((address[6] << 1) & 0x1Eu) | 0x60u);
    output[offset + 6u] = (uint8_t)(flags | (last != 0 ? 1u : 0u));
    return offset + 7u;
}

size_t h9_main_ax25_encode(const H9MainAx25Packet *packet,
                            uint8_t *output, size_t capacity)
{
    size_t index;
    size_t used;
    size_t overhead;
    if (packet == NULL || output == NULL ||
        packet->digipeater_count > H9_MAIN_AX25_MAX_DIGI ||
        (packet->information_length != 0 && packet->information == NULL))
        return 0;
    overhead = 16u + (size_t)packet->digipeater_count * 7u;
    if (packet->information_length > H9_MAIN_AX25_MAX_PACKET - overhead ||
        capacity < overhead + packet->information_length) return 0;
    used = encode_address(output, 0, packet->destination, 0);
    used = encode_address(output, used, packet->source,
                          packet->digipeater_count == 0);
    for (index = 0; index < packet->digipeater_count; ++index)
        used = encode_address(output, used, packet->digipeaters[index],
                              index + 1u == packet->digipeater_count);
    output[used++] = 0x03u;
    output[used++] = packet->pid == 0 ? 0xF0u : packet->pid;
    for (index = 0; index < packet->information_length; ++index)
        output[used++] = packet->information[index];
    return used;
}
