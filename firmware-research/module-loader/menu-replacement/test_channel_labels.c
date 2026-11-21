#include "channel_labels.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { SECTOR = 4096, DIRECTORY = 0xa1f000, BYTES = CHANNEL_LABELS_END - CHANNEL_LABELS_BANK_A };
struct flash {
    uint8_t labels[BYTES], directory[SECTOR];
    unsigned changes, cut, tear;
    uint32_t failed_read;
};

static int read_flash(void *context, uint32_t address, void *out, unsigned length)
{
    struct flash *flash = context;
    assert(!((uintptr_t)out & 3) && !(address & 31) && length == 32);
    if (flash->failed_read == address) return 0;
    if (address >= DIRECTORY && address + length <= DIRECTORY + SECTOR)
        memcpy(out, flash->directory + (address - DIRECTORY), length);
    else {
        assert(address >= CHANNEL_LABELS_BANK_A && address + length <= CHANNEL_LABELS_END);
        memcpy(out, flash->labels + (address - CHANNEL_LABELS_BANK_A), length);
    }
    return 1;
}

static int program_flash(void *context, uint32_t address, const void *data, unsigned length)
{
    struct flash *flash = context;
    uint8_t *out = flash->labels + (address - CHANNEL_LABELS_BANK_A);
    const uint8_t *in = data;
    assert(!((uintptr_t)data & 3) && !(address & 3) && (length == 4 || length == 32));
    assert(address >= CHANNEL_LABELS_BANK_A && address + length <= CHANNEL_LABELS_END);
    unsigned cut = ++flash->changes == flash->cut;
    unsigned count = cut ? flash->tear ? length / 2 : 0 : length;
    for (unsigned i = 0; i < count; ++i) {
        assert((out[i] & in[i]) == in[i]);
        out[i] &= in[i];
    }
    return !cut;
}

static int erase_flash(void *context, uint32_t address)
{
    struct flash *flash = context;
    assert(address >= CHANNEL_LABELS_BANK_A && address + SECTOR <= CHANNEL_LABELS_END && !(address & (SECTOR - 1)));
    if (++flash->changes == flash->cut) return 0;
    memset(flash->labels + (address - CHANNEL_LABELS_BANK_A), 0xff, SECTOR);
    return 1;
}

static struct channel_labels_io io_for(struct flash *flash)
{
    return (struct channel_labels_io){ flash, read_flash, program_flash, erase_flash };
}

static void blank_flash(struct flash *flash)
{
    memset(flash->labels, 0xff, sizeof flash->labels);
    memset(flash->directory, 0xff, sizeof flash->directory);
    flash->changes = flash->cut = flash->tear = 0;
    flash->failed_read = 0;
}

static void label_is(const struct channel_labels_io *io, const struct channel_labels *snapshot,
                     unsigned channel, const char *expected)
{
    char label[CHANNEL_LABELS_MAX + 1];
    assert(channel_labels_get(io, snapshot, channel, label) == CHANNEL_LABELS_OK);
    assert(!strcmp(label, expected));
}

static void round_trip(void)
{
    struct flash flash;
    blank_flash(&flash);
    struct channel_labels_io io = io_for(&flash);
    struct channel_labels current;
    assert(channel_labels_open(&io, &current) == CHANNEL_LABELS_OK && current.base == 0);
    label_is(&io, &current, 199, "");
    assert(channel_labels_set(&io, &current, 199, "") == CHANNEL_LABELS_OK && !flash.changes);
    assert(channel_labels_set(&io, &current, 0, "bad") == CHANNEL_LABELS_INVALID);
    assert(channel_labels_set(&io, &current, 200, "bad") == CHANNEL_LABELS_INVALID);
    assert(channel_labels_set(&io, &current, 1, "012345678901234567890123456789012") == CHANNEL_LABELS_INVALID);
    assert(channel_labels_set(&io, &current, 1, "not\nprintable") == CHANNEL_LABELS_INVALID);
    const char *full = "01234567890123456789012345678901";
    assert(channel_labels_set(&io, &current, 199, full) == CHANNEL_LABELS_OK);
    label_is(&io, &current, 199, full);
    label_is(&io, &current, 1, "");
    unsigned unchanged = flash.changes;
    assert(channel_labels_set(&io, &current, 199, full) == CHANNEL_LABELS_OK && flash.changes == unchanged);

    struct channel_labels stale = current;
    assert(channel_labels_set(&io, &current, 1, "Downtown repeater / CH A") == CHANNEL_LABELS_OK);
    assert(current.base == CHANNEL_LABELS_BANK_B && current.generation == 2);
    /* A second writer with an old snapshot preserves newer labels. */
    assert(channel_labels_set(&io, &stale, 199, "") == CHANNEL_LABELS_OK);
    assert(stale.base == CHANNEL_LABELS_BANK_A && stale.generation == 3);
    assert(channel_labels_open(&io, &current) == CHANNEL_LABELS_OK && current.generation == 3);
    label_is(&io, &current, 1, "Downtown repeater / CH A");
    label_is(&io, &current, 199, "");
}

static void power_cuts(void)
{
    struct flash original, interrupted;
    blank_flash(&original);
    struct channel_labels_io io = io_for(&original);
    struct channel_labels stable = { 0 };
    assert(channel_labels_set(&io, &stable, 199, "Second sector data") == CHANNEL_LABELS_OK);
    assert(channel_labels_set(&io, &stable, 1, "Before") == CHANNEL_LABELS_OK);
    /* Two erases, the header, both records, CRC and the final marker. */
    for (unsigned failure = 1; failure <= 7; ++failure) {
        for (unsigned tear = 0; tear < 2; ++tear) {
            interrupted = original;
            interrupted.changes = 0;
            interrupted.cut = failure;
            interrupted.tear = tear;
            io = io_for(&interrupted);
            struct channel_labels reloaded = stable;
            assert(channel_labels_set(&io, &reloaded, 1, "After") == CHANNEL_LABELS_IO);
            assert(reloaded.base == stable.base && reloaded.generation == stable.generation);
            assert(channel_labels_open(&io, &reloaded) == CHANNEL_LABELS_OK);
            label_is(&io, &reloaded, 1, "Before");
            label_is(&io, &reloaded, 199, "Second sector data");
            interrupted.cut = 0;
            assert(channel_labels_set(&io, &reloaded, 1, "After") == CHANNEL_LABELS_OK);
            assert(channel_labels_open(&io, &reloaded) == CHANNEL_LABELS_OK);
            label_is(&io, &reloaded, 1, "After");
            label_is(&io, &reloaded, 199, "Second sector data");
        }
    }
    blank_flash(&interrupted);
    interrupted.cut = 1;
    interrupted.tear = 1;
    io = io_for(&interrupted);
    struct channel_labels reloaded;
    assert(channel_labels_set(&io, &reloaded, 199, "First one") == CHANNEL_LABELS_IO);
    assert(channel_labels_open(&io, &reloaded) == CHANNEL_LABELS_OK);
    label_is(&io, &reloaded, 199, "");
    assert(channel_labels_set(&io, &reloaded, 199, "First one") == CHANNEL_LABELS_OK);
    label_is(&io, &reloaded, 199, "First one");
}

static void put32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (8u * i));
}

static void claim(uint8_t *directory, uint32_t sector)
{
    memset(directory, 0xff, SECTOR);
    memcpy(directory, "H9DATA01", 8);
    put32(directory + 8, 1);
    put32(directory + 12, 4);
    put32(directory + 80, 1);
    put32(directory + 84, sector);
    put32(directory + 88, 4);
    uint16_t crc = 0;
    for (unsigned i = 0; i < 92; ++i) {
        crc ^= (uint16_t)directory[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000u) ? 0x1021u : 0));
    }
    directory[92] = (uint8_t)crc;
    directory[93] = (uint8_t)(crc >> 8);
}

static void ownership_and_corruption(void)
{
    struct flash flash;
    struct channel_labels_io io = io_for(&flash);
    struct channel_labels snapshot;
    for (uint32_t sector = CHANNEL_LABELS_BANK_A; sector < CHANNEL_LABELS_END; sector += SECTOR) {
        blank_flash(&flash);
        claim(flash.directory, sector);
        assert(channel_labels_open(&io, &snapshot) == CHANNEL_LABELS_CLAIMED);
        assert(channel_labels_set(&io, &snapshot, 1, "Denied") == CHANNEL_LABELS_CLAIMED && !flash.changes);
    }
    blank_flash(&flash);
    claim(flash.directory, CHANNEL_LABELS_BANK_A - SECTOR);
    assert(channel_labels_set(&io, &snapshot, 1, "Older") == CHANNEL_LABELS_OK);
    assert(channel_labels_set(&io, &snapshot, 1, "Newer") == CHANNEL_LABELS_OK);
    flash.labels[CHANNEL_LABELS_BANK_B - CHANNEL_LABELS_BANK_A + 32] ^= 1;
    assert(channel_labels_open(&io, &snapshot) == CHANNEL_LABELS_OK && snapshot.generation == 1);
    label_is(&io, &snapshot, 1, "Older");
    assert(channel_labels_set(&io, &snapshot, 1, "Repaired") == CHANNEL_LABELS_OK);
    label_is(&io, &snapshot, 1, "Repaired");

    blank_flash(&flash);
    flash.labels[SECTOR + 128] = 0;
    assert(channel_labels_set(&io, &snapshot, 1, "Denied") == CHANNEL_LABELS_FOREIGN && !flash.changes);
    blank_flash(&flash);
    flash.directory[1024] = 0;
    assert(channel_labels_set(&io, &snapshot, 1, "Denied") == CHANNEL_LABELS_FOREIGN && !flash.changes);
    blank_flash(&flash);
    flash.failed_read = CHANNEL_LABELS_BANK_B + SECTOR;
    assert(channel_labels_set(&io, &snapshot, 1, "Denied") == CHANNEL_LABELS_IO && !flash.changes);
}

int main(void)
{
    round_trip();
    power_cuts();
    ownership_and_corruption();
    puts("Long channel labels: length, sparse writes, interruption, directory claims and CRC recovery passed.");
    return 0;
}
