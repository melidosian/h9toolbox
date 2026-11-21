#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "channel_labels.h"

static struct label_state host_state;
static uint8_t chip[CHANNEL_LABELS_END - CHANNEL_LABELS_BANK_A];
static unsigned nor_ready = 1, writes;

static void chip_read(void *out, uint32_t address, unsigned length)
{
    if (address >= 0xA1F000 && address + length <= 0xA20000) memset(out, 255, length);
    else {
        assert(address >= CHANNEL_LABELS_BANK_A && address + length <= CHANNEL_LABELS_END);
        memcpy(out, chip + address - CHANNEL_LABELS_BANK_A, length);
    }
}
static void chip_program(const void *in, uint32_t address, unsigned length)
{
    assert(address >= CHANNEL_LABELS_BANK_A && address + length <= CHANNEL_LABELS_END);
    const uint8_t *p = in;
    for (unsigned i = 0; i < length; ++i) chip[address - CHANNEL_LABELS_BANK_A + i] &= p[i];
    ++writes;
}
static void chip_erase(uint32_t sector)
{
    assert(sector >= 0x3f6 && sector < 0x3fa);
    memset(chip + (sector - 0x3f6) * 4096u, 255, 4096);
}

#define LABEL_STATE (&host_state)
#define RADIO_LABEL_READY nor_ready
#define RADIO_LABEL_READ chip_read
#define RADIO_LABEL_PROGRAM chip_program
#define RADIO_LABEL_ERASE chip_erase
#define RADIO_LABEL_LOCK() ((void)0)
#define RADIO_LABEL_UNLOCK() ((void)0)
#include "channel_labels_native.c"

static void read_block(unsigned address, uint8_t data[32])
{
    for (unsigned i = 0; i < 32; ++i) assert(radio_label_clone_read(address + i, &data[i]) == 0);
}
static void write_block(unsigned address, uint8_t data[33])
{
    unsigned sum = 0;
    for (unsigned i = 0; i < 32; ++i) sum += data[i];
    data[32] = sum;
    assert(radio_label_clone_write(address, data, 32) == 0);
}

int main(void)
{
    memset(chip, 255, sizeof chip);
    uint8_t data[32];
    uint8_t select[33] = {'H','9','L','N',1,199};
    uint8_t label[33] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
    read_block(0x3f00, data);
    assert(!memcmp(data, "H9LN\1\0\0", 7));
    assert(data[7] == 1);
    write_block(0x3f20, select);
    read_block(0x3f00, data);
    assert(!data[5] && data[6] == 199);
    read_block(0x3f40, data);
    for (unsigned i = 0; i < 32; ++i) assert(!data[i]);
    write_block(0x3f40, label);
    assert(writes);
    read_block(0x3f40, data);
    assert(!memcmp(data, label, 32));
    memset(&host_state, 0, sizeof host_state);
    write_block(0x3f20, select);
    read_block(0x3f40, data);
    assert(!memcmp(data, label, 32));

    unsigned before = writes;
    label[32] ^= 1;
    assert(radio_label_clone_write(0x3f40, label, 32) == 0);
    read_block(0x3f00, data);
    assert(data[5] == CHANNEL_LABELS_INVALID && writes == before);
    label[0] = 0; /* malformed NUL padding */
    write_block(0x3f40, label);
    read_block(0x3f00, data);
    assert(data[5] == CHANNEL_LABELS_INVALID && writes == before);
    memset(label, 0, sizeof label);
    write_block(0x3f40, label);
    read_block(0x3f40, data);
    assert(!memcmp(data, label, 32));
    puts("Native long-name clone selection, 32-byte round trip, reboot and clear passed.");
}
