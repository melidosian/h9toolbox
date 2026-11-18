#include "toolbox_settings_pair.h"
#include <stddef.h>

enum { TSP_BYTES = 4096, TSP_RECORD = 32 };
enum state { EMPTY, VALID, UNKNOWN };
struct sector_state { enum state state; uint64_t generation; uint8_t mute; };
static const uint8_t magic[8] = { 'T','S','P','A','I','R','0','1' };
static const uint8_t commit[4] = { 'D','O','N','E' };

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t x)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(x >> (i * 8));
}
static uint64_t get64(const uint8_t *p)
{
    return (uint64_t)get32(p) | (uint64_t)get32(p + 4) << 32;
}
static void put64(uint8_t *p, uint64_t x)
{
    put32(p, (uint32_t)x);
    put32(p + 4, (uint32_t)(x >> 32));
}
static uint32_t crc32(const uint8_t *p)
{
    uint32_t c = ~0u;
    for (unsigned i = 0; i < 24; ++i) {
        c ^= p[i];
        for (unsigned j = 0; j < 8; ++j)
            c = (c >> 1) ^ ((0u - (c & 1u)) & UINT32_C(0xEDB88320));
    }
    return ~c;
}
static int same(const uint8_t *a, const uint8_t *b, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) if (a[i] != b[i]) return 0;
    return 1;
}
static int blank(const uint8_t *p, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) if (p[i] != 0xFF) return 0;
    return 1;
}
static int io_valid(const struct tsp_io *io, int write)
{
    if (!io || !io->read || (write && (!io->program || !io->erase))) return 0;
    for (unsigned i = 0; i < 2; ++i)
        if ((io->sector[i] & (TSP_BYTES - 1u)) ||
            io->sector[i] > UINT32_MAX - TSP_BYTES) return 0;
    return io->sector[0] != io->sector[1];
}
/* Every read is 32 bytes into an aligned buffer, including the blank tail.
 * A blank header alone does not establish that a sector is empty. */
static enum tsp_result inspect(const struct tsp_io *io, unsigned index,
                               struct sector_state *out)
{
    uint32_t words[8];
    uint8_t *p = (uint8_t *)words;
    uint8_t header[32];
    int tail_blank = 1, all_blank;
    uint32_t base = io->sector[index];
    if (!io->read(io->ctx, base, p, TSP_RECORD)) return TSP_IO;
    for (unsigned i = 0; i < TSP_RECORD; ++i) header[i] = p[i];
    all_blank = blank(header, TSP_RECORD);
    for (unsigned off = TSP_RECORD; off < TSP_BYTES; off += TSP_RECORD) {
        if (!io->read(io->ctx, base + off, p, TSP_RECORD)) return TSP_IO;
        if (!blank(p, TSP_RECORD)) tail_blank = 0;
    }
    out->state = UNKNOWN;
    out->generation = 0;
    out->mute = 0;
    if (all_blank && tail_blank) out->state = EMPTY;
    else if (tail_blank && same(header, magic, 8) &&
             same(header + 28, commit, 4) &&
             get32(header + 24) == crc32(header) &&
             get64(header + 8) != 0 && get32(header + 16) <= 1 &&
             get32(header + 20) == 0) {
        out->state = VALID;
        out->generation = get64(header + 8);
        out->mute = (uint8_t)get32(header + 16);
    }
    return TSP_OK;
}
static enum tsp_result scan(const struct tsp_io *io, struct sector_state states[2],
                            int *newest)
{
    for (unsigned i = 0; i < 2; ++i) {
        enum tsp_result r = inspect(io, i, &states[i]);
        if (r != TSP_OK) return r;
    }
    if (states[0].state == VALID && states[1].state == VALID &&
        states[0].generation == states[1].generation) return TSP_FOREIGN;
    *newest = -1;
    for (int i = 0; i < 2; ++i)
        if (states[i].state == VALID && (*newest < 0 ||
            states[i].generation > states[*newest].generation)) *newest = i;
    return TSP_OK;
}
enum tsp_result tsp_load(const struct tsp_io *io, struct ts_settings *out)
{
    struct sector_state states[2];
    int newest;
    if (!out) return TSP_INVALID;
    out->mute = 0;
    if (!io_valid(io, 0)) return TSP_INVALID;
    enum tsp_result r = scan(io, states, &newest);
    if (r != TSP_OK) return r;
    if (newest >= 0) {
        out->mute = states[newest].mute;
        return TSP_OK;
    }
    if (states[0].state == UNKNOWN || states[1].state == UNKNOWN)
        return TSP_FOREIGN;
    return TSP_EMPTY;
}
enum tsp_result tsp_save(const struct tsp_io *io, const struct ts_settings *setting)
{
    struct sector_state states[2], verified;
    uint32_t record_words[8], check_words[8];
    uint8_t *record = (uint8_t *)record_words;
    uint8_t *check = (uint8_t *)check_words;
    int newest, target;
    uint64_t generation;
    if (!setting || setting->mute > 1 || !io_valid(io, 1)) return TSP_INVALID;
    if (!io->ownership_approved) return TSP_DENIED;
    enum tsp_result r = scan(io, states, &newest);
    if (r != TSP_OK) return r;
    if (states[0].state == UNKNOWN || states[1].state == UNKNOWN)
        return TSP_FOREIGN;
    if (newest >= 0 && states[newest].mute == setting->mute) return TSP_OK;
    if (newest >= 0 && states[newest].generation == UINT64_MAX)
        return TSP_EXHAUSTED;
    generation = newest < 0 ? 1 : states[newest].generation + 1;
    target = newest < 0 ? 0 : 1 - newest;
    uint32_t base = io->sector[target];
    if (states[target].state == VALID) {
        if (!io->erase(io->ctx, base)) return TSP_IO;
        r = inspect(io, (unsigned)target, &verified);
        if (r != TSP_OK) return r;
        if (verified.state != EMPTY) return TSP_IO;
    }
    for (unsigned i = 0; i < 32; ++i) record[i] = 0;
    for (unsigned i = 0; i < 8; ++i) record[i] = magic[i];
    put64(record + 8, generation);
    put32(record + 16, setting->mute);
    put32(record + 24, crc32(record));
    for (unsigned i = 28; i < 32; ++i) record[i] = 0xFF;
    if (!io->program(io->ctx, base, record, 28) ||
        !io->read(io->ctx, base, check, 32) || !same(record, check, 32))
        return TSP_IO;
    if (!io->program(io->ctx, base + 28, commit, 4)) return TSP_IO;
    r = inspect(io, (unsigned)target, &verified);
    if (r != TSP_OK) return r;
    if (verified.state != VALID || verified.generation != generation ||
        verified.mute != setting->mute) return TSP_IO;
    return TSP_OK;
}
