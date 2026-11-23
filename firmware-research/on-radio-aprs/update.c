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

#ifdef APRS_IMAGE_PART_1_ADDRESS
static uint8_t image_byte(const uint8_t *tail, unsigned offset)
{
    if (offset < APRS_IMAGE_PART_1_SIZE)
        return ((const uint8_t *)(uintptr_t)APRS_IMAGE_PART_1_ADDRESS)[offset];
    offset -= APRS_IMAGE_PART_1_SIZE;
    if (offset < APRS_IMAGE_PART_2_SIZE)
        return ((const uint8_t *)(uintptr_t)APRS_IMAGE_PART_2_ADDRESS)[offset];
    offset -= APRS_IMAGE_PART_2_SIZE;
#ifdef APRS_IMAGE_PART_3_ADDRESS
    if (offset < APRS_IMAGE_PART_3_SIZE)
        return ((const uint8_t *)(uintptr_t)APRS_IMAGE_PART_3_ADDRESS)[offset];
    offset -= APRS_IMAGE_PART_3_SIZE;
#endif
#ifdef APRS_IMAGE_PART_4_ADDRESS
    if (offset < APRS_IMAGE_PART_4_SIZE)
        return ((const uint8_t *)(uintptr_t)APRS_IMAGE_PART_4_ADDRESS)[offset];
    offset -= APRS_IMAGE_PART_4_SIZE;
#endif
#ifdef APRS_IMAGE_PART_5_ADDRESS
    if (offset < APRS_IMAGE_PART_5_SIZE)
        return ((const uint8_t *)(uintptr_t)APRS_IMAGE_PART_5_ADDRESS)[offset];
    offset -= APRS_IMAGE_PART_5_SIZE;
#endif
#ifdef APRS_IMAGE_PART_6_ADDRESS
    if (offset < APRS_IMAGE_PART_6_SIZE)
        return ((const uint8_t *)(uintptr_t)APRS_IMAGE_PART_6_ADDRESS)[offset];
    offset -= APRS_IMAGE_PART_6_SIZE;
#endif
#ifdef APRS_IMAGE_PART_7_ADDRESS
    if (offset < APRS_IMAGE_PART_7_SIZE)
        return ((const uint8_t *)(uintptr_t)APRS_IMAGE_PART_7_ADDRESS)[offset];
    offset -= APRS_IMAGE_PART_7_SIZE;
#endif
#ifdef APRS_IMAGE_PART_8_ADDRESS
    if (offset < APRS_IMAGE_PART_8_SIZE)
        return ((const uint8_t *)(uintptr_t)APRS_IMAGE_PART_8_ADDRESS)[offset];
    offset -= APRS_IMAGE_PART_8_SIZE;
#endif
    return tail[offset];
}

uint32_t aprs_image_crc32(const uint8_t *tail)
{
    uint32_t crc = UINT32_MAX;
    for (unsigned offset = 0; offset < APRS_IMAGE_SIZE; ++offset) {
        crc ^= image_byte(tail, offset);
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xedb88320) : 0);
    }
    return ~crc;
}
#else
#define image_byte(image, offset) ((image)[offset])
#define aprs_image_crc32(image) aprs_crc32(image, APRS_IMAGE_SIZE)
#endif

enum aprs_result aprs_program(const uint8_t *image, uint32_t checksum, aprs_transact transact)
{
    if (aprs_image_crc32(image) != checksum)
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
            payload[4 + i] = image_byte(image, offset + i);
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
