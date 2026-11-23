#ifndef H9_CHANNEL_LABELS_NATIVE_H
#define H9_CHANNEL_LABELS_NATIVE_H
#include "radio_settings.h"
extern const struct radio_storage radio_long_name_storage;
int radio_label_get(unsigned channel, char label[33]);
unsigned radio_label_revision(void);

/* Type-C v1.0.38 memory selection fields shared by the stock label hook and
 * the Toolbox channel preview. Callers supply side 0 or 1. */
#define radio_memory_channel(byte, side) \
    ((byte(0x11AE9u + (side)) && byte(0x12BB2u + (side)) == 1) ? byte(0x11AEFu + (side)) : 0)
int radio_label_clone_read(unsigned address, uint8_t *out);
int radio_label_clone_write(unsigned address, const uint8_t *data, unsigned length);
#endif
