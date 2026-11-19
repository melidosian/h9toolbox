#ifndef H9_APRS_UPDATE_H
#define H9_APRS_UPDATE_H

#include <stddef.h>
#include <stdint.h>

enum {
    APRS_IMAGE_SIZE = 17304,
    APRS_BLOCK_SIZE = 128,
    APRS_WRITE = 0x0d,
    APRS_EXIT = 0x55
};

enum aprs_result {
    APRS_OK, APRS_BAD_IMAGE, APRS_BLOCK_TIMEOUT, APRS_EXIT_TIMEOUT
};

typedef int (*aprs_transact)(uint8_t command, unsigned size, const uint8_t *payload);

uint32_t aprs_crc32(const uint8_t *data, size_t size);
enum aprs_result aprs_program(const uint8_t *image, uint32_t checksum, aprs_transact transact);

struct aprs_version {
    unsigned size;
    uint8_t in_frame;
};

int aprs_version_feed(struct aprs_version *parser, uint8_t byte);

#endif
