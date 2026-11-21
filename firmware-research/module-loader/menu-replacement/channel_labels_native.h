#ifndef H9_CHANNEL_LABELS_NATIVE_H
#define H9_CHANNEL_LABELS_NATIVE_H
#include "radio_settings.h"
extern const struct radio_storage radio_long_name_storage;
int radio_label_get(unsigned channel, char label[33]);
unsigned radio_label_revision(void);
int radio_label_clone_read(unsigned address, uint8_t *out);
int radio_label_clone_write(unsigned address, const uint8_t *data, unsigned length);
#endif
