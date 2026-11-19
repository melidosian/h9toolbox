#include "toolbox_settings_pair.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { BASE = 0x3FC000, BYTES = 8192, GAME = 0xA1F000 };
struct chip {
    uint8_t data[BYTES];
    uint8_t game[4096];
    int budget, fail_read, drop_program, drop_erase;
    unsigned programs, erases;
};
static void reset(struct chip *c)
{
    memset(c, 0, sizeof *c);
    memset(c->data, 0xFF, sizeof c->data);
    memset(c->game, 0xFF, sizeof c->game);
    c->budget = -1;
}
static int step(struct chip *c)
{
    if (c->budget == 0) return 0;
    if (c->budget > 0) --c->budget;
    return 1;
}
static unsigned offset(uint32_t a, unsigned n)
{
    assert(a >= BASE && a <= BASE + BYTES && n <= BASE + BYTES - a);
    return a - BASE;
}
static int read_chip(void *v, uint32_t a, void *p, unsigned n)
{
    struct chip *c = v;
    if (c->fail_read) return 0;
    assert(n == 32); /* target NOR reads 32 bytes into aligned storage */
    assert(((uintptr_t)p & 3u) == 0);
    if (a >= GAME && a <= GAME + sizeof c->game - n)
        memcpy(p, c->game + (a - GAME), n);
    else
        memcpy(p, c->data + offset(a, n), n);
    return 1;
}
static int program_chip(void *v, uint32_t a, const void *p, unsigned n)
{
    struct chip *c = v;
    const uint8_t *b = p;
    unsigned off = offset(a, n);
    ++c->programs;
    for (unsigned i = 0; i < n; ++i) {
        if (!step(c)) return 0;
        if (!c->drop_program) c->data[off + i] &= b[i];
    }
    return 1;
}
static int erase_chip(void *v, uint32_t a)
{
    struct chip *c = v;
    unsigned off = offset(a, 4096);
    assert(!(a & 4095u));
    ++c->erases;
    for (unsigned i = 0; i < 4096; ++i) {
        if (!step(c)) return 0;
        if (!c->drop_erase) c->data[off + i] = 0xFF;
    }
    return 1;
}
static struct tsp_io io_for(struct chip *c)
{
    struct tsp_io io = { c, read_chip, program_chip, erase_chip,
                         { BASE, BASE + 4096 }, 1 };
    return io;
}
static void load_is(struct tsp_io *io, enum tsp_result result, unsigned mute)
{
    struct ts_settings got = { .mute = 77 };
    assert(tsp_load(io, &got) == result);
    assert(got.mute == mute);
}
static void tests(void)
{
    struct chip c;
    reset(&c);
    struct tsp_io io = io_for(&c);
    const struct ts_settings off = { .minutes = TS_DEFAULT_MINUTES };
    const struct ts_settings on = { .mute = 1, .minutes = TS_DEFAULT_MINUTES };
    const struct ts_settings invalid = { .mute = 2, .minutes = TS_DEFAULT_MINUTES };
    load_is(&io, TSP_EMPTY, 0);
    io.ownership_approved = 0;
    assert(tsp_save(&io, &on) == TSP_DENIED && !c.programs && !c.erases);
    io.ownership_approved = 1;
    c.data[BYTES - 1] = 0;
    assert(tsp_save(&io, &on) == TSP_FOREIGN && !c.programs);
    reset(&c);
    static const uint8_t v1_record[32] = {
        0x54,0x53,0x50,0x41,0x49,0x52,0x30,0x31,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x8d,0x59,0x66,0xa1,0x44,0x4f,0x4e,0x45
    }; /* First TSPAIR01 mute snapshot from the original writer. */
    memcpy(c.data, v1_record, sizeof v1_record);
    struct ts_settings legacy;
    assert(tsp_load(&io, &legacy) == TSP_OK && legacy.mute == 1 && !legacy.armed &&
           legacy.minutes == TS_DEFAULT_MINUTES && !legacy.tx_frequency);
    reset(&c);
    assert(tsp_save(&io, &on) == TSP_OK);
    assert(!memcmp(c.data, "TSPAIR02", 8));
    load_is(&io, TSP_OK, 1);
    unsigned writes = c.programs;
    assert(tsp_save(&io, &on) == TSP_OK && c.programs == writes);
    struct chip first = c;
    assert(tsp_save(&io, &off) == TSP_OK);
    load_is(&io, TSP_OK, 0);
    struct chip second = c;

    /* A torn initial claim never reports an enabled setting. */
    for (int cut = 0; cut < 32; ++cut) {
        reset(&c);
        c.budget = cut;
        assert(tsp_save(&io, &on) == TSP_IO);
        struct ts_settings got = { .mute = 1, .armed = 1 };
        assert(tsp_load(&io, &got) != TSP_OK && !got.mute);
        assert(!got.armed);
    }
    /* Every byte cut through erase, payload, and commit retains the previous
     * committed value, even when the inactive sector becomes unknown. */
    for (int cut = 0; cut < 4096 + 32; ++cut) {
        c = second;
        c.budget = cut;
        assert(tsp_save(&io, &on) == TSP_IO);
        struct ts_settings got;
        enum tsp_result result = tsp_load(&io, &got);
        assert((result == TSP_OK || result == TSP_DEGRADED) && !got.mute);
    }
    c = second;
    assert(tsp_save(&io, &on) == TSP_OK);
    load_is(&io, TSP_OK, 1);

    /* A bad latest sector may be read past, but cannot be auto-erased. */
    for (unsigned bit = 0; bit < 32u * 8u; ++bit) {
        c = second;
        c.data[4096 + bit / 8] ^= (uint8_t)(1u << (bit % 8));
        load_is(&io, TSP_DEGRADED, 1);
        writes = c.programs;
        assert(tsp_save(&io, &off) == TSP_FOREIGN);
        assert(c.programs == writes && !c.erases);
    }
    c = first;
    memcpy(c.data + 4096, c.data, 4096);
    load_is(&io, TSP_FOREIGN, 0); /* duplicate generations */
    assert(tsp_save(&io, &off) == TSP_FOREIGN);
    c = first;
    c.drop_program = 1;
    assert(tsp_save(&io, &off) == TSP_IO);
    c.drop_program = 0;
    load_is(&io, TSP_OK, 1);
    c = second;
    c.drop_erase = 1;
    assert(tsp_save(&io, &on) == TSP_IO);
    load_is(&io, TSP_OK, 0);
    c = first;
    c.fail_read = 1;
    assert(tsp_save(&io, &off) == TSP_IO && !c.erases);
    load_is(&io, TSP_IO, 0);
    c = first;
    for (unsigned i = 0; i < 80; ++i) {
        struct ts_settings next = { .mute = (uint8_t)(i & 1u), .minutes = TS_DEFAULT_MINUTES };
        assert(tsp_save(&io, &next) == TSP_OK);
        load_is(&io, TSP_OK, next.mute);
    }
    assert(c.erases == 79);
    reset(&c);
    static const uint8_t max_generation[32] = {
        0x54,0x53,0x50,0x41,0x49,0x52,0x30,0x31,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x26,0x45,0x60,0x98,0x44,0x4f,0x4e,0x45
    };
    memcpy(c.data, max_generation, sizeof max_generation);
    load_is(&io, TSP_OK, 1);
    assert(tsp_save(&io, &off) == TSP_EXHAUSTED && !c.programs && !c.erases);
    assert(tsp_save(&io, &invalid) == TSP_INVALID);
    io.sector[1] = io.sector[0];
    load_is(&io, TSP_INVALID, 0);
    assert(tsp_save(&io, &on) == TSP_INVALID);
}

static void reboot_preserves_aprs_preferences(void)
{
    struct chip c;
    reset(&c);
    struct tsp_io io = io_for(&c);
    struct ts_settings saved = { .mute = 1, .armed = 1, .timed = 1, .minutes = 10,
        .symbol = 2, .tx_frequency = 14439000, .tx_setting = 3, .tx_side = 1 };
    assert(tsp_save(&io, &saved) == TSP_OK);
    struct ts_settings active = { 0 };
    assert(tsp_load(&io, &active) == TSP_OK && active.mute && active.armed && active.timed &&
           active.minutes == 10 && active.symbol == 2 && active.tx_frequency == 14439000 &&
           active.tx_setting == 3 && active.tx_side == 1);
    active.armed = 0;
    c.budget = 29;
    assert(tsp_save(&io, &active) == TSP_IO);
    assert(tsp_load(&io, &active) == TSP_DEGRADED && active.armed && active.mute);

    reset(&c);
    assert(tsp_save(&io, &saved) == TSP_OK);
    saved.armed = 0;
    assert(tsp_save(&io, &saved) == TSP_OK);
    assert(tsp_load(&io, &active) == TSP_OK && !active.armed && active.mute);
    saved.mute = 0;
    assert(tsp_save(&io, &saved) == TSP_OK);
    assert(tsp_load(&io, &active) == TSP_OK && !active.armed && !active.mute &&
           active.minutes == 10 && active.symbol == 2);
    saved.armed = 1;
    saved.tx_frequency = 0;
    assert(tsp_save(&io, &saved) == TSP_INVALID);
    saved.tx_frequency = 43550000;
    assert(tsp_save(&io, &saved) == TSP_INVALID);
}

static void game_directory(uint8_t *p, uint32_t start, uint32_t length, unsigned complete)
{
    memset(p, 0xFF, 4096);
    memset(p, 0, 94);
    memcpy(p, "H9DATA01", 8);
    p[8] = p[80] = 1;
    for (unsigned i = 0; i < 4; ++i) {
        p[12 + i] = p[88 + i] = (uint8_t)(length >> (8 * i));
        p[84 + i] = (uint8_t)(start >> (8 * i));
    }
    uint16_t crc = 0;
    for (unsigned i = 0; i < 92; ++i) {
        crc ^= (uint16_t)p[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000u) ? 0x1021u : 0));
    }
    p[92] = (uint8_t)crc;
    p[93] = (uint8_t)(crc >> 8);
    if (complete) memcpy(p + 4096 - 32, "H9DATA-COMPLETE", 15);
}

static void respects_game_ownership(void)
{
    struct chip c;
    reset(&c);
    struct tsp_io io = io_for(&c);
    assert(tsp_check_game_directory(&io, GAME) == TSP_OK);
    c.game[4095] = 0;
    assert(tsp_check_game_directory(&io, GAME) == TSP_FOREIGN);
    game_directory(c.game, 0xA20000, 1056192, 1);
    assert(tsp_check_game_directory(&io, GAME) == TSP_OK);
    game_directory(c.game, BASE - 4096, 4096, 1);
    assert(tsp_check_game_directory(&io, GAME) == TSP_OK);
    game_directory(c.game, BASE - 4096, 4097, 1);
    assert(tsp_check_game_directory(&io, GAME) == TSP_DENIED);
    game_directory(c.game, BASE + 4096, 1, 0);
    assert(tsp_check_game_directory(&io, GAME) == TSP_DENIED);
    c.game[92] ^= 1;
    assert(tsp_check_game_directory(&io, GAME) == TSP_FOREIGN);
    c.fail_read = 1;
    assert(tsp_check_game_directory(&io, GAME) == TSP_IO);
    assert(!c.programs && !c.erases);
}

int main(void)
{
    tests();
    reboot_preserves_aprs_preferences();
    respects_game_ownership();
    puts("toolbox settings pair: rollover and power cuts PASS");
    return 0;
}
