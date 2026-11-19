#include "toolbox_settings_pair.h"
#include <stddef.h>

enum { TSP_BYTES = 4096, TSP_RECORD = 32 };
enum state { EMPTY, VALID, UNKNOWN };
struct sector_state { enum state state; uint64_t generation; uint32_t config, frequency; };
static const uint8_t magic[7] = { 'T','S','P','A','I','R','0' };
static const uint8_t commit[4] = { 'D','O','N','E' };

/* TSPAIR02 config is little endian: flags, minutes, symbol, TX selector.
 * Flags 0–3 are mute, armed, timed and TX side B. The next word is TX frequency. */
static uint32_t pack(const struct ts_settings *s)
{
    return s->mute | (uint32_t)s->armed << 1 | (uint32_t)s->timed << 2 |
           (uint32_t)s->tx_side << 3 | (uint32_t)s->minutes << 8 |
           (uint32_t)s->symbol << 16 | (uint32_t)s->tx_setting << 24;
}

static void unpack(struct ts_settings *s, uint32_t config, uint32_t frequency)
{
    s->mute = config & 1u;
    s->armed = (config >> 1) & 1u;
    s->timed = (config >> 2) & 1u;
    s->tx_side = (config >> 3) & 1u;
    s->minutes = (uint8_t)(config >> 8);
    s->symbol = (uint8_t)(config >> 16);
    s->tx_setting = (uint8_t)(config >> 24);
    s->tx_frequency = frequency;
}

static int valid_config(uint32_t config, uint32_t frequency)
{
    unsigned minutes = (config >> 8) & 255u;
    return !(config & 0xf0u) && minutes && minutes <= 30 && (config >> 24) <= 4 &&
           ((config >> 16) & 255u) <= 2 &&
           ((!frequency && !(config & 2u)) || (frequency >= 14400000u && frequency <= 14800000u));
}

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

enum tsp_result tsp_check_game_directory(const struct tsp_io *io, uint32_t directory)
{
    uint32_t words[32];
    uint8_t *header = (uint8_t *)words;
    if (!io_valid(io, 0) || (directory & (TSP_BYTES - 1u)) ||
        directory >= 0x1000000u) return TSP_INVALID;
    for (unsigned i = 0; i < sizeof words; i += TSP_RECORD)
        if (!io->read(io->ctx, directory + i, header + i, TSP_RECORD)) return TSP_IO;
    if (blank(header, sizeof words)) {
        for (unsigned i = sizeof words; i < TSP_BYTES; i += TSP_RECORD) {
            if (!io->read(io->ctx, directory + i, header, TSP_RECORD)) return TSP_IO;
            if (!blank(header, TSP_RECORD)) return TSP_FOREIGN;
        }
        return TSP_OK;
    }

    uint32_t count = get32(header + 80);
    if (!same(header, (const uint8_t *)"H9DATA01", 8) || get32(header + 8) != 1 ||
        count == 0 || count > 3) return TSP_FOREIGN;
    unsigned end = 84 + 8 * count;
    uint16_t crc = 0;
    for (unsigned i = 0; i < end; ++i) {
        crc ^= (uint16_t)header[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000u) ? 0x1021u : 0));
    }
    if (crc != (header[end] | header[end + 1] << 8)) return TSP_FOREIGN;

    uint32_t total = 0;
    for (unsigned i = 0; i < count; ++i) {
        uint32_t start = get32(header + 84 + 8 * i), length = get32(header + 88 + 8 * i);
        if (!length || (start & (TSP_BYTES - 1u)) || start >= 0x1000000u ||
            length > 0x1000000u - start) return TSP_FOREIGN;
        uint32_t limit = (start + length + TSP_BYTES - 1u) & ~(TSP_BYTES - 1u);
        if (!((start >= 0xA20000u && limit <= 0xCC0000u) ||
              (start >= 0x717000u && limit <= 0x800000u) ||
              (start >= 0x3A7000u && limit <= 0x400000u))) return TSP_FOREIGN;
        for (unsigned j = 0; j < 2; ++j)
            if (start < io->sector[j] + TSP_BYTES && limit > io->sector[j]) return TSP_DENIED;
        total += length;
    }
    return total == get32(header + 12) ? TSP_OK : TSP_FOREIGN;
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
    out->config = out->frequency = 0;
    if (all_blank && tail_blank) out->state = EMPTY;
    else if (tail_blank && same(header, magic, sizeof magic) &&
             same(header + 28, commit, 4) &&
             get32(header + 24) == crc32(header) &&
             get64(header + 8) != 0) {
        uint32_t config = get32(header + 16), frequency = get32(header + 20);
        if (header[7] == '1' && config <= 1 && frequency == 0) {
            config |= TS_DEFAULT_MINUTES << 8;
        } else if (header[7] != '2' || !valid_config(config, frequency)) {
            return TSP_OK;
        }
        out->state = VALID;
        out->generation = get64(header + 8);
        out->config = config;
        out->frequency = frequency;
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
    unpack(out, TS_DEFAULT_MINUTES << 8, 0);
    if (!io_valid(io, 0)) return TSP_INVALID;
    enum tsp_result r = scan(io, states, &newest);
    if (r != TSP_OK) return r;
    if (newest < 0)
        return states[0].state == UNKNOWN || states[1].state == UNKNOWN ? TSP_FOREIGN : TSP_EMPTY;
    unpack(out, states[newest].config, states[newest].frequency);
    return states[1 - newest].state == UNKNOWN ? TSP_DEGRADED : TSP_OK;
}
enum tsp_result tsp_save(const struct tsp_io *io, const struct ts_settings *setting)
{
    struct sector_state states[2], verified;
    uint32_t record_words[8], check_words[8];
    uint8_t *record = (uint8_t *)record_words;
    uint8_t *check = (uint8_t *)check_words;
    int newest, target;
    uint64_t generation;
    if (!setting || setting->mute > 1 || setting->armed > 1 || setting->timed > 1 ||
        setting->tx_side > 1 || !io_valid(io, 1)) return TSP_INVALID;
    uint32_t config = pack(setting), frequency = setting->tx_frequency;
    if (!valid_config(config, frequency)) return TSP_INVALID;
    if (!io->ownership_approved) return TSP_DENIED;
    enum tsp_result r = scan(io, states, &newest);
    if (r != TSP_OK) return r;
    if (states[0].state == UNKNOWN || states[1].state == UNKNOWN)
        return TSP_FOREIGN;
    if (newest >= 0 && states[newest].config == config && states[newest].frequency == frequency)
        return TSP_OK;
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
    for (unsigned i = 0; i < sizeof magic; ++i) record[i] = magic[i];
    record[7] = '2';
    put64(record + 8, generation);
    put32(record + 16, config);
    put32(record + 20, frequency);
    put32(record + 24, crc32(record));
    for (unsigned i = 28; i < 32; ++i) record[i] = 0xFF;
    if (!io->program(io->ctx, base, record, 28) ||
        !io->read(io->ctx, base, check, 32) || !same(record, check, 32))
        return TSP_IO;
    if (!io->program(io->ctx, base + 28, commit, 4)) return TSP_IO;
    r = inspect(io, (unsigned)target, &verified);
    if (r != TSP_OK) return r;
    if (verified.state != VALID || verified.generation != generation ||
        verified.config != config || verified.frequency != frequency) return TSP_IO;
    return TSP_OK;
}
