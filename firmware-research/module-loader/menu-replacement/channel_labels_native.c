#include "channel_labels_native.h"
#include "channel_labels.h"

enum { LABEL_SELECT = 0x3F20, LABEL_DATA = 0x3F40, LABEL_BLOCK = 32 };
static const uint8_t signature[] = { 'H', '9', 'L', 'N', 1 };

struct label_state {
    struct channel_labels store;
    uint32_t revision;
    unsigned busy, opened;
    uint8_t channel, result, reply[LABEL_BLOCK];
};
/* Protected by the unified loader's 0x29900 heap boundary. */
#ifndef LABEL_STATE
#define LABEL_STATE ((struct label_state *)0x29904u)
#define RADIO_LABEL_READY (*(const volatile uint16_t *)0x12234u == 0x5AA5u)
#define RADIO_LABEL_READ ((void (*)(void *, uint32_t, unsigned))0x01e4ff22u)
#define RADIO_LABEL_PROGRAM ((void (*)(const void *, uint32_t, unsigned))0x01e5006eu)
#define RADIO_LABEL_ERASE ((void (*)(uint32_t))0x01e50000u)
#define RADIO_LABEL_LOCK() ((void (*)(void))0x02001634u)()
#define RADIO_LABEL_UNLOCK() ((void (*)(void))0x02001650u)()
#endif
_Static_assert(sizeof(struct label_state) <= 0x7C, "channel label service RAM");

static int ready(void)
{ return RADIO_LABEL_READY; }

static int read_nor(void *ctx, uint32_t address, void *dst, unsigned length)
{
    (void)ctx;
    if (!ready()) return 0;
    RADIO_LABEL_READ(dst, address, length);
    return 1;
}

static int program_nor(void *ctx, uint32_t address, const void *src, unsigned length)
{
    (void)ctx;
    if (!ready() || address < CHANNEL_LABELS_BANK_A || length > CHANNEL_LABELS_END - address ||
        address >= CHANNEL_LABELS_END) return 0;
    RADIO_LABEL_PROGRAM(src, address, length);
    return 1;
}

static int erase_nor(void *ctx, uint32_t address)
{
    (void)ctx;
    if (!ready() || address < CHANNEL_LABELS_BANK_A || address >= CHANNEL_LABELS_END ||
        (address & 0xfffu)) return 0;
    RADIO_LABEL_ERASE(address >> 12);
    return 1;
}

static const struct channel_labels_io nor = { 0, read_nor, program_nor, erase_nor };

static int acquire(void)
{
    RADIO_LABEL_LOCK();
    unsigned free = !LABEL_STATE->busy;
    if (free) LABEL_STATE->busy = 1;
    RADIO_LABEL_UNLOCK();
    return free;
}

unsigned radio_label_revision(void) { return LABEL_STATE->revision; }

int radio_label_get(unsigned channel, char label[33])
{
    label[0] = 0;
    if (!acquire()) return CHANNEL_LABELS_IO;
    int result = CHANNEL_LABELS_OK;
    if (!LABEL_STATE->opened) {
        result = channel_labels_open(&nor, &LABEL_STATE->store);
        LABEL_STATE->opened = result == CHANNEL_LABELS_OK;
    }
    if (!result) result = channel_labels_get(&nor, &LABEL_STATE->store, channel, label);
    LABEL_STATE->busy = 0;
    return result;
}

static int save_label(unsigned channel, const char *label)
{
    if (!acquire()) return CHANNEL_LABELS_IO;
    int result = channel_labels_set(&nor, &LABEL_STATE->store, channel, label);
    if (!result) {
        ++LABEL_STATE->revision;
        LABEL_STATE->opened = 1;
    }
    LABEL_STATE->busy = 0;
    return result;
}

static int read_label(unsigned channel, void *dst, unsigned length)
{
    char label[33];
    if (length != LABEL_BLOCK) return RADIO_INVALID;
    if (radio_label_get(channel, label)) return RADIO_IO_ERROR;
    uint8_t *data = dst;
    unsigned i = 0;
    for (; i < length && label[i]; ++i) data[i] = (uint8_t)label[i];
    for (; i < length; ++i) data[i] = 0;
    return RADIO_OK;
}

static int write_label(unsigned channel, const void *src, unsigned length)
{
    if (length != LABEL_BLOCK) return CHANNEL_LABELS_INVALID;
    const uint8_t *data = src;
    char label[LABEL_BLOCK + 1];
    unsigned ended = 0;
    for (unsigned i = 0; i < LABEL_BLOCK; ++i) {
        unsigned ch = data[i];
        if (ch && (ended || ch < 32 || ch > 126)) return CHANNEL_LABELS_INVALID;
        ended |= ch == 0;
        label[i] = (char)ch;
    }
    label[LABEL_BLOCK] = 0;
    return save_label(channel, label);
}

static int write_setting(unsigned channel, const void *src, unsigned length)
{
    int result = write_label(channel, src, length);
    return !result ? RADIO_OK : result == CHANNEL_LABELS_INVALID ? RADIO_INVALID : RADIO_IO_ERROR;
}

const struct radio_storage radio_long_name_storage = { read_label, write_setting };

/* The original clone handlers call the read hook a byte at a time. Cache a
 * row at byte zero so a 32-byte response uses exactly one NOR record read. */
int radio_label_clone_read(unsigned address, uint8_t *out)
{
    unsigned byte = address & (LABEL_BLOCK - 1);
    if (address < LABEL_SELECT) {
        uint8_t value = 0;
        if (byte < sizeof signature) value = signature[byte];
        else if (byte == 5) value = LABEL_STATE->result;
        else if (byte == 6) value = LABEL_STATE->channel;
        else if (byte == 7) value = 1; /* sector-aware serial clone writes */
        *out = value;
    } else if (address < LABEL_DATA) *out = 0;
    else {
        if (!byte) {
            for (unsigned i = 0; i < LABEL_BLOCK; ++i) LABEL_STATE->reply[i] = 0;
            LABEL_STATE->result = read_label(LABEL_STATE->channel, LABEL_STATE->reply, LABEL_BLOCK);
        }
        *out = LABEL_STATE->reply[byte];
    }
    return 0;
}

int radio_label_clone_write(unsigned address, const uint8_t *data, unsigned length)
{
    unsigned sum = 0;
    if (length != LABEL_BLOCK) {
        LABEL_STATE->result = CHANNEL_LABELS_INVALID;
        return 0;
    }
    for (unsigned i = 0; i < LABEL_BLOCK; ++i) sum += data[i];
    if ((sum & 255) != data[LABEL_BLOCK]) {
        LABEL_STATE->result = CHANNEL_LABELS_INVALID;
        return 0;
    }

    if (address == LABEL_SELECT) {
        LABEL_STATE->channel = 0;
        LABEL_STATE->result = CHANNEL_LABELS_INVALID;
        for (unsigned i = 0; i < sizeof signature; ++i)
            if (data[i] != signature[i]) return 0;
        for (unsigned i = 6; i < LABEL_BLOCK; ++i)
            if (data[i]) return 0;
        if (!data[5] || data[5] > CHANNEL_LABELS_COUNT) return 0;
        LABEL_STATE->channel = data[5];
        LABEL_STATE->result = CHANNEL_LABELS_OK;
    } else LABEL_STATE->result = write_label(LABEL_STATE->channel, data, length);
    return 0; /* stock clone still ACKs, result is separately read via HELLO */
}
