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

/* Stock app+0x05D9A6 discards trailing shifted spaces and keeps SSID/H bits. */
static void decode_address(const uint8_t input[7], uint8_t output[7])
{
    size_t index;
    size_t last_nonspace = 0;
    for (index = 0; index < 6u; ++index) {
        output[index] = (uint8_t)(input[index] >> 1);
        if (output[index] != ' ' && output[index] != 0)
            last_nonspace = index + 1u;
    }
    for (index = last_nonspace; index < 6u; ++index) output[index] = 0;
    output[6] = (uint8_t)(((input[6] & 0x1Eu) >> 1) |
                           (input[6] & 0x80u));
}

int h9_main_ax25_decode(const uint8_t *packet, size_t length,
                        H9MainAx25Decoded *decoded)
{
    size_t cursor = 14u;
    size_t remaining;
    uint8_t last_address;
    if (packet == NULL || decoded == NULL || length < 16u) return 0;
    decode_address(packet, decoded->destination);
    decode_address(packet + 7u, decoded->source);
    decoded->digipeater_count = 0;
    last_address = (uint8_t)(packet[13] & 1u);
    while (!last_address) {
        if (decoded->digipeater_count == H9_MAIN_AX25_MAX_DIGI ||
            length - cursor < 9u) return 0;
        decode_address(packet + cursor,
                       decoded->digipeaters[decoded->digipeater_count]);
        last_address = (uint8_t)(packet[cursor + 6u] & 1u);
        decoded->digipeater_count++;
        cursor += 7u;
    }
    if (length - cursor < 2u) return 0;
    decoded->control = packet[cursor++];
    decoded->pid = packet[cursor++];
    remaining = length - cursor;
    decoded->information_length = remaining < H9_MAIN_AX25_MAX_INFORMATION ?
                                  remaining : H9_MAIN_AX25_MAX_INFORMATION;
    for (size_t index = 0; index < decoded->information_length; ++index)
        decoded->information[index] = packet[cursor + index];
    return 1;
}

int h9_main_ax25_is_aprs_payload(const H9MainAx25Decoded *decoded)
{
    return decoded != NULL && decoded->information_length != 0u &&
           (decoded->pid == 0xF0u || (decoded->pid & 0xF0u) == 0x30u);
}
