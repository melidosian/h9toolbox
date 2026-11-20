#ifndef H9_CHANNEL_LABELS_H
#define H9_CHANNEL_LABELS_H

#include <stdint.h>

#define CHANNEL_LABELS_BANK_A 0x3F6000u
#define CHANNEL_LABELS_BANK_B 0x3F8000u
#define CHANNEL_LABELS_END    0x3FA000u
#define CHANNEL_LABELS_COUNT 199u
#define CHANNEL_LABELS_MAX 32u

/* IO callbacks return nonzero on success. Erase receives a sector address
 * in bytes. Reads use aligned 32-byte buffers, and programs use 32-byte
 * blocks or aligned four-byte metadata fields. */
struct channel_labels_io {
    void *ctx;
    int (*read)(void *, uint32_t, void *, unsigned);
    int (*program)(void *, uint32_t, const void *, unsigned);
    int (*erase)(void *, uint32_t);
};

struct channel_labels {
    uint32_t base, generation;
};

enum channel_labels_result {
    CHANNEL_LABELS_OK, CHANNEL_LABELS_INVALID, CHANNEL_LABELS_IO,
    CHANNEL_LABELS_CLAIMED, CHANNEL_LABELS_FOREIGN, CHANNEL_LABELS_EXHAUSTED
};

/* Open once outside the draw path. An empty store opens successfully.
 * Discard a cached snapshot after another writer saves to the same store. */
enum channel_labels_result channel_labels_open(const struct channel_labels_io *, struct channel_labels *);

/* Channel numbers are 1..199. Optional labels are NUL-terminated printable
 * ASCII. An empty string clears the label and leaves the native name alone. */
enum channel_labels_result channel_labels_get(const struct channel_labels_io *, const struct channel_labels *,
                                              unsigned channel, char label[CHANNEL_LABELS_MAX + 1]);
enum channel_labels_result channel_labels_set(const struct channel_labels_io *, struct channel_labels *,
                                              unsigned channel, const char *label);

#endif
