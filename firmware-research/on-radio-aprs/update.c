#include "update.h"
#include "stage-config.h"

uint32_t aprs_crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = UINT32_MAX;
    while (size--) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xedb88320) : 0);
    }
    return ~crc;
}

enum aprs_result aprs_program(const uint8_t *image, uint32_t checksum, aprs_transact transact)
{
    if (aprs_crc32(image, APRS_IMAGE_SIZE) != checksum)
        return APRS_BAD_IMAGE;

    uint8_t payload[4 + APRS_BLOCK_SIZE];
    for (unsigned offset = 0; offset < APRS_IMAGE_SIZE; offset += APRS_BLOCK_SIZE) {
        const unsigned remaining = APRS_IMAGE_SIZE - offset;
        const unsigned count = remaining < APRS_BLOCK_SIZE ? remaining : APRS_BLOCK_SIZE;
        payload[0] = (uint8_t)(offset >> 24);
        payload[1] = (uint8_t)(offset >> 16);
        payload[2] = (uint8_t)(offset >> 8);
        payload[3] = (uint8_t)offset;
        for (unsigned i = 0; i < count; ++i)
            payload[4 + i] = image[offset + i];
        if (!transact(APRS_WRITE, 4 + count, payload))
            return APRS_BLOCK_TIMEOUT;
    }
    return transact(APRS_EXIT, 0, NULL) ? APRS_OK : APRS_EXIT_TIMEOUT;
}

int aprs_version_feed(struct aprs_version *parser, uint8_t byte)
{
    static const uint8_t reply[] = "\x21\x00" APRS_MODEM_VERSION "\0V1.0.0";
    if (byte == 0xc0) {
        const int verified = parser->in_frame && parser->size == sizeof(reply);
        parser->size = 0;
        parser->in_frame = 1;
        return verified;
    }
    if (parser->in_frame && parser->size < sizeof(reply) && byte == reply[parser->size]) {
        ++parser->size;
    } else {
        parser->in_frame = 0;
    }
    return 0;
}
