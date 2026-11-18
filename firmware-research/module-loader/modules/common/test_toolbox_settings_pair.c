#include "toolbox_settings_pair.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { BASE = 0x3FC000, BYTES = 8192 };
struct chip {
    uint8_t data[BYTES];
    int budget, fail_read, drop_program, drop_erase;
    unsigned programs, erases;
};
static void reset(struct chip *c)
{
    memset(c, 0, sizeof *c);
    memset(c->data, 0xFF, sizeof c->data);
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
    struct ts_settings got = { 77 };
    assert(tsp_load(io, &got) == result);
    assert(got.mute == mute);
}
static void tests(void)
{
    struct chip c;
    reset(&c);
    struct tsp_io io = io_for(&c);
    const struct ts_settings on = { 1 }, off = { 0 }, invalid = { 2 };
    load_is(&io, TSP_EMPTY, 0);
    io.ownership_approved = 0;
    assert(tsp_save(&io, &on) == TSP_DENIED && !c.programs && !c.erases);
    io.ownership_approved = 1;
    c.data[BYTES - 1] = 0;
    assert(tsp_save(&io, &on) == TSP_FOREIGN && !c.programs);
    reset(&c);
    assert(tsp_save(&io, &on) == TSP_OK);
    static const uint8_t python_record[32] = {
        0x54,0x53,0x50,0x41,0x49,0x52,0x30,0x31,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x8d,0x59,0x66,0xa1,0x44,0x4f,0x4e,0x45
    }; /* settings_pair.py's first ON snapshot, zlib CRC32 */
    assert(!memcmp(c.data, python_record, sizeof python_record));
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
        struct ts_settings got = { 1 };
        assert(tsp_load(&io, &got) != TSP_OK && !got.mute);
    }
    /* Every byte cut through erase, payload, and commit retains the previous
     * committed value, even when the inactive sector becomes unknown. */
    for (int cut = 0; cut < 4096 + 32; ++cut) {
        c = second;
        c.budget = cut;
        assert(tsp_save(&io, &on) == TSP_IO);
        load_is(&io, TSP_OK, 0);
    }
    c = second;
    assert(tsp_save(&io, &on) == TSP_OK);
    load_is(&io, TSP_OK, 1);

    /* A bad latest sector may be read past, but cannot be auto-erased. */
    for (unsigned bit = 0; bit < 32u * 8u; ++bit) {
        c = second;
        c.data[4096 + bit / 8] ^= (uint8_t)(1u << (bit % 8));
        load_is(&io, TSP_OK, 1);
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
        struct ts_settings next = { (uint8_t)(i & 1u) };
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
int main(void)
{
    tests();
    puts("toolbox settings pair: rollover and power cuts PASS");
    return 0;
}
