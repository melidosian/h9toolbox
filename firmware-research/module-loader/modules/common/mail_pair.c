#include "mail_pair.h"

enum { SECTOR_BYTES = 4096, HEADER = 32, ENTRY = 64, BODY = HEADER + MAIL_ITEMS * ENTRY,
       COMMIT_AT = 444, RECORD_BYTES = 448 };
enum state { EMPTY, VALID, PARTIAL, FOREIGN };
struct found { enum state state; uint64_t generation; char owner[10]; };
static const uint8_t magic[8] = { 'H','9','M','A','I','L','0','1' };
static const uint8_t done[4] = { 'D','O','N','E' };

static int eq(const uint8_t *a, const uint8_t *b, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) if (a[i] != b[i]) return 0;
    return 1;
}
static int blank(const uint8_t *p, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) if (p[i] != 0xFF) return 0;
    return 1;
}
static void copy(uint8_t *dst, const uint8_t *src, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) dst[i] = src[i];
}
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}
static uint64_t get64(const uint8_t *p) { return get32(p) | (uint64_t)get32(p + 4) << 32; }
static void put64(uint8_t *p, uint64_t v) { put32(p, (uint32_t)v); put32(p + 4, (uint32_t)(v >> 32)); }
static uint32_t checksum(const uint8_t *p)
{
    uint32_t crc = ~0u;
    for (unsigned i = 0; i < BODY; ++i) {
        if (i >= 28 && i < 32) continue;
        crc ^= p[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((0u - (crc & 1u)) & UINT32_C(0xEDB88320));
    }
    return ~crc;
}
static int terminated(const uint8_t *p, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) if (!p[i]) return 1;
    return 0;
}
static int valid_data(const uint8_t *p)
{
    if (!terminated(p + 16, 10) || p[26] > MAIL_ITEMS) return 0;
    for (unsigned i = 0; i < p[26]; ++i) {
        const uint8_t *m = p + HEADER + i * ENTRY;
        if (!terminated(m, 10) || !terminated(m + 10, 10) ||
            !terminated(m + 20, 40) || (m[60] & ~7u) || (m[60] & 6u) == 6u || m[61] > 99) return 0;
    }
    return 1;
}
static int io_valid(const struct tsp_io *io, int writing)
{
    return io && io->read && (!writing || (io->program && io->erase && io->ownership_approved)) &&
           io->sector[0] == MAIL_SECTOR_A && io->sector[1] == MAIL_SECTOR_B;
}
static enum mail_result ownership(const struct tsp_io *io)
{
    enum tsp_result r = tsp_check_game_directory(io, MAIL_GAME_DIRECTORY);
    return r == TSP_OK ? MAIL_OK : r == TSP_DENIED ? MAIL_CLAIMED :
           r == TSP_IO ? MAIL_IO : MAIL_FOREIGN;
}

/* Every NOR read has an aligned destination and a 32-byte length. An erased
 * header alone never proves that the rest of a sector is ours to erase. */
static enum mail_result inspect(const struct tsp_io *io, unsigned side,
                                uint32_t words[RECORD_BYTES / 4], struct found *out)
{
    uint8_t *p = (uint8_t *)words;
    uint32_t tail[8];
    uint32_t base = io->sector[side];
    for (unsigned at = 0; at < RECORD_BYTES; at += 32)
        if (!io->read(io->ctx, base + at, p + at, 32)) return MAIL_IO;
    int tail_blank = 1;
    for (unsigned at = RECORD_BYTES; at < SECTOR_BYTES; at += 32) {
        if (!io->read(io->ctx, base + at, tail, 32)) return MAIL_IO;
        if (!blank((const uint8_t *)tail, 32)) tail_blank = 0;
    }
    out->generation = 0;
    out->state = FOREIGN;
    if (blank(p, RECORD_BYTES) && tail_blank) out->state = EMPTY;
    else if (tail_blank && eq(p, magic, 8) && blank(p + BODY, COMMIT_AT - BODY)) {
        out->state = PARTIAL;
        copy((uint8_t *)out->owner, p + 16, 10);
        if (eq(p + COMMIT_AT, done, 4) && get64(p + 8) &&
            get32(p + 28) == checksum(p) && valid_data(p)) {
            out->state = VALID;
            out->generation = get64(p + 8);
            copy((uint8_t *)out->owner, p + 16, 10);
        }
    }
    return MAIL_OK;
}
static enum mail_result scan(const struct tsp_io *io, uint32_t words[RECORD_BYTES / 4],
                             struct found found[2], int *newest)
{
    for (unsigned i = 0; i < 2; ++i) {
        enum mail_result r = inspect(io, i, words, &found[i]);
        if (r != MAIL_OK) return r;
    }
    if (found[0].state == VALID && found[1].state == VALID &&
        found[0].generation == found[1].generation) return MAIL_FOREIGN;
    *newest = -1;
    for (int i = 0; i < 2; ++i)
        if (found[i].state == VALID && (*newest < 0 ||
            found[i].generation > found[*newest].generation)) *newest = i;
    return MAIL_OK;
}
static void decode(const uint8_t *p, struct mail_snapshot *s)
{
    copy((uint8_t *)s->owner, p + 16, 10);
    s->count = p[26]; s->next_id = p[27];
    for (unsigned i = 0; i < s->count; ++i) {
        const uint8_t *m = p + HEADER + i * ENTRY;
        copy((uint8_t *)s->item[i].from, m, 10);
        copy((uint8_t *)s->item[i].to, m + 10, 10);
        copy((uint8_t *)s->item[i].text, m + 20, 40);
        s->item[i].flags = m[60]; s->item[i].id = m[61];
    }
}
static void encode(uint8_t *p, const struct mail_snapshot *s, uint64_t generation)
{
    for (unsigned i = 0; i < RECORD_BYTES; ++i) p[i] = i < BODY ? 0 : 0xFF;
    copy(p, magic, 8); put64(p + 8, generation);
    copy(p + 16, (const uint8_t *)s->owner, 10);
    p[26] = s->count; p[27] = s->next_id;
    for (unsigned i = 0; i < s->count; ++i) {
        uint8_t *m = p + HEADER + i * ENTRY;
        copy(m, (const uint8_t *)s->item[i].from, 10);
        copy(m + 10, (const uint8_t *)s->item[i].to, 10);
        copy(m + 20, (const uint8_t *)s->item[i].text, 40);
        m[60] = s->item[i].flags; m[61] = s->item[i].id;
    }
    put32(p + 28, checksum(p));
}
static int valid_snapshot(const struct mail_snapshot *s)
{
    if (!s || !terminated((const uint8_t *)s->owner, 10) || s->count > MAIL_ITEMS) return 0;
    for (unsigned i = 0; i < s->count; ++i) {
        const struct mail_entry *m = &s->item[i];
        if (!terminated((const uint8_t *)m->from, 10) || !terminated((const uint8_t *)m->to, 10) ||
            !terminated((const uint8_t *)m->text, 40) || (m->flags & ~7u) ||
            (m->flags & 6u) == 6u || m->id > 99) return 0;
    }
    return 1;
}

enum mail_result mail_load(const struct tsp_io *io, struct mail_snapshot *out)
{
    uint32_t words[RECORD_BYTES / 4];
    struct found found[2];
    int newest;
    if (!out || !io_valid(io, 0)) return MAIL_INVALID;
    enum mail_result r = ownership(io);
    if (r != MAIL_OK) return r;
    r = scan(io, words, found, &newest);
    if (r != MAIL_OK) return r;
    if (newest < 0) {
        if (found[0].state == EMPTY && found[1].state == EMPTY) return MAIL_EMPTY;
        unsigned partial = found[0].state == PARTIAL ? 0u : 1u;
        if (found[partial].state == PARTIAL && found[1 - partial].state == EMPTY &&
            terminated((const uint8_t *)found[partial].owner, 10)) {
            copy((uint8_t *)out->owner, (const uint8_t *)found[partial].owner, 10);
            out->count = out->next_id = 0;
            return MAIL_RESTARTABLE;
        }
        return MAIL_DAMAGED;
    }
    r = inspect(io, (unsigned)newest, words, &found[newest]);
    if (r != MAIL_OK || found[newest].state != VALID) return r == MAIL_OK ? MAIL_DAMAGED : r;
    decode((const uint8_t *)words, out);
    return found[1 - newest].state == FOREIGN ? MAIL_FOREIGN : MAIL_OK;
}

enum mail_result mail_save(const struct tsp_io *io, const struct mail_snapshot *next)
{
    uint32_t words[RECORD_BYTES / 4], check[8];
    struct found found[2], verified;
    int newest;
    if (!io_valid(io, 1) || !valid_snapshot(next)) return MAIL_INVALID;
    enum mail_result r = ownership(io);
    if (r != MAIL_OK) return r;
    r = scan(io, words, found, &newest);
    if (r != MAIL_OK) return r;
    if (found[0].state == FOREIGN || found[1].state == FOREIGN) return MAIL_FOREIGN;
    if (newest < 0 && (found[0].state != EMPTY || found[1].state != EMPTY)) {
        /* A first write may lose power before its commit marker. It can be
         * restarted only when the other sector is blank and its owner matches. */
        unsigned partial = found[0].state == PARTIAL ? 0u : 1u;
        if (found[partial].state != PARTIAL || found[1 - partial].state != EMPTY ||
            !terminated((const uint8_t *)found[partial].owner, 10) ||
            !eq((const uint8_t *)found[partial].owner, (const uint8_t *)next->owner, 10))
            return MAIL_DAMAGED;
    }
    if (newest >= 0 && !eq((const uint8_t *)found[newest].owner,
                           (const uint8_t *)next->owner, 10)) return MAIL_OWNER;
    if (newest >= 0 && found[newest].generation == UINT64_MAX) return MAIL_EXHAUSTED;
    unsigned target = newest < 0 ? 0u : (unsigned)(1 - newest);
    uint32_t base = io->sector[target];
    if (found[target].state != EMPTY) {
        if (!io->erase(io->ctx, base)) return MAIL_IO;
        r = inspect(io, target, words, &verified);
        if (r != MAIL_OK || verified.state != EMPTY) return MAIL_IO;
    }
    encode((uint8_t *)words, next, newest < 0 ? 1 : found[newest].generation + 1);
    for (unsigned at = 0; at < BODY; at += 32) {
        if (!io->program(io->ctx, base + at, (uint8_t *)words + at, 32) ||
            !io->read(io->ctx, base + at, check, 32) ||
            !eq((const uint8_t *)check, (uint8_t *)words + at, 32)) return MAIL_IO;
    }
    if (!io->program(io->ctx, base + COMMIT_AT, done, 4)) return MAIL_IO;
    r = inspect(io, target, words, &verified);
    return r != MAIL_OK ? r : verified.state == VALID ? MAIL_OK : MAIL_IO;
}
