#include "mail_pair.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { GAME = MAIL_GAME_DIRECTORY };
static uint8_t sectors[8192], directory[4096];
static unsigned writes, fail_at;

static int read_nor(void *unused, uint32_t at, void *dst, unsigned len)
{
    (void)unused;
    assert(len == 32 && !((uintptr_t)dst & 3u));
    if (at >= GAME && at + len <= GAME + sizeof directory)
        memcpy(dst, directory + at - GAME, len);
    else if (at >= MAIL_SECTOR_A && at + len <= MAIL_SECTOR_A + sizeof sectors)
        memcpy(dst, sectors + at - MAIL_SECTOR_A, len);
    else return 0;
    return 1;
}
static int program_nor(void *unused, uint32_t at, const void *src, unsigned len)
{
    (void)unused;
    if (++writes == fail_at) return 0;
    assert(at >= MAIL_SECTOR_A && at + len <= MAIL_SECTOR_A + sizeof sectors);
    for (unsigned i = 0; i < len; ++i) sectors[at - MAIL_SECTOR_A + i] &= ((const uint8_t *)src)[i];
    return 1;
}
static int erase_nor(void *unused, uint32_t at)
{
    (void)unused;
    assert(at == MAIL_SECTOR_A || at == MAIL_SECTOR_B);
    memset(sectors + at - MAIL_SECTOR_A, 0xFF, 4096);
    return 1;
}
static struct tsp_io io = { .read = read_nor, .program = program_nor, .erase = erase_nor,
    .sector = { MAIL_SECTOR_A, MAIL_SECTOR_B }, .ownership_approved = 1 };

static void reset(void)
{
    memset(sectors, 0xFF, sizeof sectors);
    memset(directory, 0xFF, sizeof directory);
    writes = fail_at = 0;
}
static struct mail_snapshot make(const char *message)
{
    struct mail_snapshot s = {0};
    strcpy(s.owner, "N0CALL-7"); s.count = 1; s.next_id = 13;
    strcpy(s.item[0].from, "N0CALL-7");
    strcpy(s.item[0].to, "W1AW");
    strcpy(s.item[0].text, message);
    s.item[0].id = 12;
    return s;
}
static void game_claims_mail(void)
{
    memset(directory, 0xFF, sizeof directory);
    memset(directory, 0, 94);
    memcpy(directory, "H9DATA01", 8);
    directory[8] = 1; directory[12] = 0x00; directory[13] = 0x10;
    directory[80] = 1;                 /* one 4096-byte segment */
    directory[84] = (uint8_t)MAIL_SECTOR_A;
    directory[85] = (uint8_t)(MAIL_SECTOR_A >> 8);
    directory[86] = (uint8_t)(MAIL_SECTOR_A >> 16);
    directory[88] = 0x00; directory[89] = 0x10;
    uint16_t crc = 0;
    for (unsigned i = 0; i < 92; ++i) {
        crc ^= (uint16_t)directory[i] << 8;
        for (unsigned b = 0; b < 8; ++b)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000u) ? 0x1021u : 0));
    }
    directory[92] = (uint8_t)crc; directory[93] = (uint8_t)(crc >> 8);
}

int main(void)
{
    reset();
    struct mail_snapshot got = {0}, first = make("Meet at three"), second = make("On my way");
    assert(mail_load(&io, &got) == MAIL_EMPTY);
    assert(mail_save(&io, &first) == MAIL_OK);
    assert(mail_load(&io, &got) == MAIL_OK && got.count == 1 &&
           !strcmp(got.item[0].text, "Meet at three") && got.next_id == 13);
    second.item[0].flags = 4; /* recipient rejected the message */
    assert(mail_save(&io, &second) == MAIL_OK);
    assert(mail_load(&io, &got) == MAIL_OK && !strcmp(got.item[0].text, "On my way") && got.item[0].flags == 4);

    /* An interrupted alternate write leaves the previous commit readable. */
    writes = 0; fail_at = 3;
    assert(mail_save(&io, &first) == MAIL_IO);
    assert(mail_load(&io, &got) == MAIL_OK && !strcmp(got.item[0].text, "On my way"));
    fail_at = 0;
    assert(mail_save(&io, &first) == MAIL_OK);
    assert(mail_load(&io, &got) == MAIL_OK && !strcmp(got.item[0].text, "Meet at three"));

    struct mail_snapshot other = make("Wrong owner");
    strcpy(other.owner, "K1ABC");
    assert(mail_save(&io, &other) == MAIL_OWNER);
    reset(); writes = 0; fail_at = 2;
    assert(mail_save(&io, &first) == MAIL_IO);
    fail_at = 0;
    assert(mail_load(&io, &got) == MAIL_RESTARTABLE && !strcmp(got.owner, first.owner));
    assert(mail_save(&io, &other) == MAIL_DAMAGED); /* do not steal partial ownership */
    assert(mail_save(&io, &first) == MAIL_OK);
    assert(mail_load(&io, &got) == MAIL_OK && !strcmp(got.item[0].text, "Meet at three"));
    reset(); sectors[4096 + 1000] = 0;
    assert(mail_save(&io, &first) == MAIL_FOREIGN);
    reset(); game_claims_mail();
    assert(mail_load(&io, &got) == MAIL_CLAIMED);
    assert(mail_save(&io, &first) == MAIL_CLAIMED);
    puts("mail pair: recovery, ownership and directory guards PASS");
}
