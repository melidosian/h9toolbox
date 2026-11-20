#include "channel_labels.h"
#include "../modules/common/toolbox_settings_pair.h"

enum {
    SECTOR = 4096, BANK = 2 * SECTOR, BLOCK = 32,
    GEN_AT = 8, CRC_AT = 12, COMMIT_AT = 16, BANK_BODY = (CHANNEL_LABELS_COUNT + 1) * BLOCK
};
enum bank_state { BANK_EMPTY, BANK_VALID, BANK_PARTIAL, BANK_DAMAGED, BANK_FOREIGN };

static const uint8_t magic[8] = { 'H', '9', 'L', 'A', 'B', 'E', 'L', '1' };
static const uint32_t committed = UINT32_C(0x454e4f44); /* DONE, little-endian */
static const uint32_t banks[2] = { CHANNEL_LABELS_BANK_A, CHANNEL_LABELS_BANK_B };

_Static_assert(CHANNEL_LABELS_MAX == BLOCK, "each label occupies one block");
_Static_assert(CHANNEL_LABELS_BANK_B == CHANNEL_LABELS_BANK_A + BANK &&
               CHANNEL_LABELS_END == CHANNEL_LABELS_BANK_B + BANK &&
               BANK_BODY < BANK, "channel label bank geometry");

static int same(const uint8_t *a, const uint8_t *b, unsigned length)
{
    for (unsigned i = 0; i < length; ++i)
        if (a[i] != b[i]) return 0;
    return 1;
}

static int erased(const uint8_t *data, unsigned length)
{
    for (unsigned i = 0; i < length; ++i)
        if (data[i] != 0xff) return 0;
    return 1;
}

static int printable(unsigned c) { return c >= 0x20 && c <= 0x7e; }

static int valid_record(const uint8_t *record)
{
    if (erased(record, BLOCK)) return 1;
    unsigned i = 0;
    while (i < BLOCK && printable(record[i])) ++i;
    if (!i) return 0;
    while (i < BLOCK && !record[i]) ++i;
    return i == BLOCK;
}

static uint32_t get32(const uint8_t *data)
{
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 |
           (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static void put32(uint8_t *data, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) data[i] = (uint8_t)(value >> (8u * i));
}

static uint32_t crc_block(uint32_t crc, const uint8_t *data, unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((0u - (crc & 1u)) & UINT32_C(0xedb88320));
    }
    return crc;
}

static enum channel_labels_result ownership(const struct channel_labels_io *io)
{
    /* The directory checker takes two sector addresses. Check the banks
     * separately, including the second sector of each and unfinished packs. */
    for (unsigned i = 0; i < 2; ++i) {
        struct tsp_io check = {
            .ctx = io->ctx, .read = io->read, .sector = { banks[i], banks[i] + SECTOR }
        };
        enum tsp_result result = tsp_check_game_directory(&check, 0xA1F000u);
        if (result != TSP_OK)
            return result == TSP_DENIED ? CHANNEL_LABELS_CLAIMED :
                result == TSP_IO ? CHANNEL_LABELS_IO : CHANNEL_LABELS_FOREIGN;
    }
    return CHANNEL_LABELS_OK;
}

static enum channel_labels_result inspect(const struct channel_labels_io *io, unsigned bank,
                                          enum bank_state *state, uint32_t *generation)
{
    uint32_t header_words[BLOCK / 4], words[BLOCK / 4];
    uint8_t *header = (uint8_t *)header_words, *data = (uint8_t *)words;
    uint32_t base = banks[bank];
    if (!io->read(io->ctx, base, header, BLOCK)) return CHANNEL_LABELS_IO;
    unsigned owned = same(header, magic, sizeof magic);
    unsigned data_blank = 1, tail_blank = 1, records_valid = 1;
    uint32_t crc = crc_block(~UINT32_C(0), header, CRC_AT);
    for (unsigned offset = BLOCK; offset < BANK; offset += BLOCK) {
        if (!io->read(io->ctx, base + offset, data, BLOCK)) return CHANNEL_LABELS_IO;
        if (offset < BANK_BODY) {
            data_blank &= erased(data, BLOCK);
            if (owned) {
                records_valid &= valid_record(data);
                crc = crc_block(crc, data, BLOCK);
            }
        } else tail_blank &= erased(data, BLOCK);
    }
    *generation = 0;
    *state = BANK_FOREIGN;
    if (data_blank && tail_blank && erased(header, BLOCK)) *state = BANK_EMPTY;
    else if (owned && tail_blank && erased(header + COMMIT_AT + 4, BLOCK - COMMIT_AT - 4)) {
        uint32_t marker = get32(header + COMMIT_AT);
        /* A torn commit can have any subset of its intended zero bits. */
        if ((marker & committed) == committed && marker != committed) *state = BANK_PARTIAL;
        else if (marker == committed) {
            *state = BANK_DAMAGED;
            uint32_t sequence = get32(header + GEN_AT);
            if (sequence && records_valid && get32(header + CRC_AT) == ~crc) {
                *state = BANK_VALID;
                *generation = sequence;
            }
        }
    }
    return CHANNEL_LABELS_OK;
}

static enum channel_labels_result scan(const struct channel_labels_io *io, struct channel_labels *selected,
                                       enum bank_state states[2])
{
    uint32_t generation[2];
    enum channel_labels_result result = ownership(io);
    if (result != CHANNEL_LABELS_OK) return result;
    for (unsigned i = 0; i < 2; ++i) {
        result = inspect(io, i, &states[i], &generation[i]);
        if (result != CHANNEL_LABELS_OK) return result;
    }
    if (generation[0] && generation[0] == generation[1]) return CHANNEL_LABELS_FOREIGN;
    if (!generation[0] && !generation[1]) {
        /* A first interrupted save may be restarted from the empty peer.
         * Do not silently reclaim a corrupt committed or unknown bank. */
        if ((states[0] == BANK_EMPTY && (states[1] == BANK_EMPTY || states[1] == BANK_PARTIAL)) ||
            (states[1] == BANK_EMPTY && states[0] == BANK_PARTIAL)) return CHANNEL_LABELS_OK;
        return CHANNEL_LABELS_FOREIGN;
    }
    unsigned newest = generation[1] > generation[0];
    *selected = (struct channel_labels){ banks[newest], generation[newest] };
    return CHANNEL_LABELS_OK;
}

enum channel_labels_result channel_labels_open(const struct channel_labels_io *io, struct channel_labels *selected)
{
    if (!selected) return CHANNEL_LABELS_INVALID;
    *selected = (struct channel_labels){ 0, 0 };
    if (!io || !io->read) return CHANNEL_LABELS_INVALID;
    enum bank_state states[2];
    return scan(io, selected, states);
}

static int valid_channel(unsigned channel) { return channel && channel <= CHANNEL_LABELS_COUNT; }

enum channel_labels_result channel_labels_get(const struct channel_labels_io *io, const struct channel_labels *selected,
                                              unsigned channel, char label[CHANNEL_LABELS_MAX + 1])
{
    if (!label) return CHANNEL_LABELS_INVALID;
    label[0] = 0;
    if (!io || !io->read || !selected || !valid_channel(channel) ||
        (!!selected->base != !!selected->generation)) return CHANNEL_LABELS_INVALID;
    if (!selected->base) return CHANNEL_LABELS_OK;
    if (selected->base != CHANNEL_LABELS_BANK_A && selected->base != CHANNEL_LABELS_BANK_B)
        return CHANNEL_LABELS_INVALID;

    uint32_t words[BLOCK / 4];
    const uint8_t *data = (const uint8_t *)words;
    if (!io->read(io->ctx, selected->base + channel * BLOCK, words, BLOCK)) return CHANNEL_LABELS_IO;
    if (!valid_record(data)) return CHANNEL_LABELS_FOREIGN;
    if (!erased(data, BLOCK)) {
        unsigned i = 0;
        for (; i < BLOCK && data[i]; ++i) label[i] = (char)data[i];
        label[i] = 0;
    }
    return CHANNEL_LABELS_OK;
}

static int program_checked(const struct channel_labels_io *io, uint32_t address,
                           const uint8_t *data, unsigned length)
{
    uint32_t read_start = address & ~(uint32_t)(BLOCK - 1u);
    uint32_t words[BLOCK / 4];
    return io->program(io->ctx, address, data, length) &&
           io->read(io->ctx, read_start, words, BLOCK) &&
           same(data, (const uint8_t *)words + (address - read_start), length);
}

enum channel_labels_result channel_labels_set(const struct channel_labels_io *io, struct channel_labels *selected,
                                              unsigned channel, const char *label)
{
    if (!io || !io->read || !io->program || !io->erase || !selected || !valid_channel(channel) || !label)
        return CHANNEL_LABELS_INVALID;
    uint32_t input_words[BLOCK / 4];
    uint8_t *input = (uint8_t *)input_words;
    unsigned length = 0;
    while (length <= BLOCK && label[length]) {
        if (length == BLOCK || !printable((uint8_t)label[length])) return CHANNEL_LABELS_INVALID;
        input[length] = (uint8_t)label[length];
        ++length;
    }
    for (unsigned i = length; i < BLOCK; ++i) input[i] = length ? 0 : 0xff;

    struct channel_labels latest = { 0, 0 };
    enum bank_state states[2];
    enum channel_labels_result result = scan(io, &latest, states);
    if (result != CHANNEL_LABELS_OK) return result;
    if (states[0] == BANK_FOREIGN || states[1] == BANK_FOREIGN) return CHANNEL_LABELS_FOREIGN;
    uint32_t words[BLOCK / 4];
    uint8_t *data = (uint8_t *)words;
    if (latest.base && !io->read(io->ctx, latest.base + channel * BLOCK, data, BLOCK))
        return CHANNEL_LABELS_IO;
    if (latest.base ? same(input, data, BLOCK) : !length) {
        *selected = latest;
        return CHANNEL_LABELS_OK;
    }
    if (latest.generation == UINT32_MAX) return CHANNEL_LABELS_EXHAUSTED;

    unsigned target = latest.base == CHANNEL_LABELS_BANK_A || (!latest.base && states[1] == BANK_PARTIAL);
    uint32_t base = banks[target];
    if (states[target] != BANK_EMPTY) {
        /* Keep the signature in the first sector intact until the second
         * has been reclaimed. The committed peer remains readable. */
        if (!io->erase(io->ctx, base + SECTOR) || !io->erase(io->ctx, base))
            return CHANNEL_LABELS_IO;
        enum bank_state state;
        uint32_t generation;
        if (inspect(io, target, &state, &generation) != CHANNEL_LABELS_OK || state != BANK_EMPTY)
            return CHANNEL_LABELS_IO;
    }
    for (unsigned i = 0; i < BLOCK; ++i) data[i] = i < sizeof magic ? magic[i] : 0xff;
    put32(data + GEN_AT, latest.generation + 1);
    uint32_t crc = crc_block(~UINT32_C(0), data, CRC_AT);
    if (!program_checked(io, base, data, BLOCK)) return CHANNEL_LABELS_IO;
    for (unsigned i = 1; i <= CHANNEL_LABELS_COUNT; ++i) {
        const uint8_t *entry = input;
        if (i != channel) {
            entry = data;
            if (latest.base) {
                if (!io->read(io->ctx, latest.base + i * BLOCK, data, BLOCK)) return CHANNEL_LABELS_IO;
            } else for (unsigned n = 0; n < BLOCK; ++n) data[n] = 0xff;
        }
        crc = crc_block(crc, entry, BLOCK);
        if (!erased(entry, BLOCK) && !program_checked(io, base + i * BLOCK, entry, BLOCK)) return CHANNEL_LABELS_IO;
    }

    put32(data, ~crc);
    if (!program_checked(io, base + CRC_AT, data, 4)) return CHANNEL_LABELS_IO;
    put32(data, committed);
    if (!program_checked(io, base + COMMIT_AT, data, 4)) return CHANNEL_LABELS_IO;
    enum bank_state state;
    uint32_t generation;
    if (inspect(io, target, &state, &generation) != CHANNEL_LABELS_OK || state != BANK_VALID ||
        generation != latest.generation + 1) return CHANNEL_LABELS_IO;
    *selected = (struct channel_labels){ base, generation };
    return CHANNEL_LABELS_OK;
}
