#ifndef RADIO_SETTINGS_H
#define RADIO_SETTINGS_H
#include <stdint.h>

enum radio_kind {
    RADIO_CHOICE, RADIO_NUMBER, RADIO_TEXT, RADIO_FREQUENCY, RADIO_CHANNEL_FREQUENCY, RADIO_DEGREES,
    RADIO_ALTITUDE, RADIO_SIGNED16, RADIO_DTMF_FIXED, RADIO_DTMF_TERMINATED, RADIO_DTMF_LENGTH
};
enum radio_result { RADIO_OK, RADIO_IO_ERROR, RADIO_INVALID, RADIO_CONFLICT, RADIO_VERIFY_ERROR, RADIO_BUSY };
enum { RADIO_MAX_VALUE = 64 };
struct radio_choice { unsigned value; const char *label; };
struct radio_setting {
    const char *id, *label;
    uint16_t offset;
    uint8_t length, kind, group, mask, shift;
    int32_t min, max;
    const struct radio_choice *choices;
    unsigned choice_count;
    uint16_t check_rx_offset; /* nonzero: enabling this memory requires valid RX */
};
struct radio_storage {
    int (*read)(unsigned offset, void *dst, unsigned length);
    int (*write)(unsigned offset, const void *src, unsigned length);
};
struct radio_edit {
    const struct radio_setting *setting;
    uint8_t original[RADIO_MAX_VALUE], value[RADIO_MAX_VALUE];
    uint8_t channel_33cm;
};
static inline int radio_is_frequency(const struct radio_setting *s)
{ return s->kind == RADIO_FREQUENCY || s->kind == RADIO_CHANNEL_FREQUENCY; }
static inline int radio_is_text(const struct radio_setting *s)
{ return s->kind == RADIO_TEXT || s->kind >= RADIO_DTMF_FIXED; }
static inline unsigned radio_text_length(const struct radio_setting *s)
{ return s->length - (s->kind == RADIO_DTMF_TERMINATED || s->kind == RADIO_DTMF_LENGTH); }
static inline int radio_is_signed(const struct radio_setting *s)
{ return s->kind == RADIO_ALTITUDE || s->kind == RADIO_SIGNED16; }
unsigned radio_value(const struct radio_edit *edit);
int radio_33cm_enabled(const struct radio_storage *storage);
int radio_begin(const struct radio_storage *storage, const struct radio_setting *setting, struct radio_edit *edit);
int radio_adjust(struct radio_edit *edit, int direction);
int radio_set_number(struct radio_edit *edit, unsigned number);
int radio_set_text(struct radio_edit *edit, const char *text, unsigned length);
int radio_save(const struct radio_storage *storage, struct radio_edit *edit);
const char *radio_value_label(const struct radio_edit *edit, char buffer[RADIO_MAX_VALUE + 1]);
const char *radio_frequency_label(unsigned frequency, char text[10]);
const char *radio_number_label(unsigned number, char *buffer); /* at most 11 bytes */
const struct radio_setting *radio_setting_by_id(const char *id);
int radio_editor(const struct radio_storage *storage, const struct radio_setting *setting);
int radio_channel_editor(const struct radio_storage *storage, uint16_t accent);
extern const struct radio_storage radio_stock_storage;
#endif
