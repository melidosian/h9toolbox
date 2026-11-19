#include "update.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t image[APRS_IMAGE_SIZE];
static unsigned blocks;
static unsigned exits;
static unsigned fail_block;
static int fail_exit;

static int check_command(uint8_t command, unsigned size, const uint8_t *payload)
{
    const unsigned offset = blocks * APRS_BLOCK_SIZE;
    if (command == APRS_EXIT) {
        assert(offset >= sizeof(image));
        assert(size == 0 && payload == NULL);
        ++exits;
        return !fail_exit;
    }
    assert(command == APRS_WRITE);
    assert(offset < sizeof(image));
    assert(payload[0] == 0 && payload[1] == 0);
    assert(payload[2] == (uint8_t)(offset >> 8) && payload[3] == (uint8_t)offset);
    const unsigned expected = blocks == 135 ? 24 : 128;
    assert(size == expected + 4);
    assert(memcmp(payload + 4, image + offset, expected) == 0);
    return ++blocks != fail_block;
}

static unsigned parse(struct aprs_version *parser, const uint8_t *bytes, size_t size)
{
    unsigned verified = 0;
    for (size_t i = 0; i < size; ++i)
        verified += aprs_version_feed(parser, bytes[i]);
    return verified;
}

static void check_version(void)
{
    static const uint8_t reply[] = {0xc0, 0x21, 0, 'V', '1', '.', '0', '.', '1', '5', 'X',
                                    0, 'V', '1', '.', '0', '.', '0', 0, 0xc0};
    struct aprs_version parser = {0};
    assert(parse(&parser, reply + 1, sizeof(reply) - 1) == 0);
    assert(parse(&parser, reply, 12) == 0);
    assert(parse(&parser, reply + 12, sizeof(reply) - 12) == 1);
    assert(parse(&parser, reply, sizeof(reply) - 2) == 0);
    assert(aprs_version_feed(&parser, 0xc0) == 0);

    uint8_t invalid[sizeof(reply)];
    memcpy(invalid, reply, sizeof(reply));
    invalid[1] = 0x20;
    assert(parse(&parser, invalid, sizeof(invalid)) == 0);
    memcpy(invalid, reply, sizeof(reply));
    invalid[9] = '4';
    assert(parse(&parser, invalid, sizeof(invalid)) == 0);
    assert(parse(&parser, reply, sizeof(reply)) == 1);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *modem = fopen(argv[1], "rb");
    assert(modem);
    assert(fread(image, 1, sizeof(image), modem) == sizeof(image));
    assert(fgetc(modem) == EOF && fclose(modem) == 0);
    assert(aprs_crc32((const uint8_t *)"123456789", 9) == 0xcbf43926);
    const uint32_t checksum = aprs_crc32(image, sizeof(image));

    assert(aprs_program(image, checksum, check_command) == APRS_OK);
    assert(blocks == 136 && exits == 1);
    blocks = exits = 0;
    image[127] ^= 1;
    assert(aprs_program(image, checksum, check_command) == APRS_BAD_IMAGE);
    assert(blocks == 0 && exits == 0);
    image[127] ^= 1;
    fail_block = 3;
    assert(aprs_program(image, checksum, check_command) == APRS_BLOCK_TIMEOUT);
    assert(blocks == 3 && exits == 0);
    blocks = fail_block = 0;
    fail_exit = 1;
    assert(aprs_program(image, checksum, check_command) == APRS_EXIT_TIMEOUT);
    assert(blocks == 136 && exits == 1);

    check_version();
    puts("Passed APRS image, 136 block writes, timeout, and boot reply tests");
    return 0;
}
