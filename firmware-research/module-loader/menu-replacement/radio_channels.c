#include "radio_channels.h"
#include "radio_tones.h"

static const struct radio_choice onoff[] = { {0,"OFF"}, {1,"ON"} };
static const struct radio_choice bandwidth[] = { {0,"WIDE"}, {1,"NARROW"} };
static const struct radio_choice power[] = { {0,"LOW"}, {1,"MID"}, {2,"HIGH"} };
static const struct radio_choice pttid[] = { {0,"OFF"}, {1,"BOT"}, {2,"EOT"}, {3,"BOTH"} };
static const struct radio_choice modulation[] = { {0,"FM"}, {1,"AM"} };
static const struct radio_choice direction[] = { {0,"OFF"}, {1,"MINUS"}, {2,"PLUS"} };

int radio_channel_field(unsigned target, unsigned field, struct radio_setting *s)
{
    if (!target || target > RADIO_CHANNEL_TARGETS || field >= RADIO_CHANNEL_FIELDS) return RADIO_INVALID;
    unsigned record = target <= 199 ? target * 16 : target == 200 ? 0x1950 : 0x1960;
    *s = (struct radio_setting){ .id = "channel", .mask = 255, .length = 1 };
    s->offset = record;
    switch (field) {
    case 0: case 1:
        s->label = field ? "TX (MHz)" : "RX (MHz)";
        s->offset += field * 4; s->length = 4; s->kind = RADIO_CHANNEL_FREQUENCY;
        s->min = 1800000; s->max = 60000000;
        if (field == 1 && target > 199) {
            s->label = "TX OFFSET (10Hz)";
            s->offset = radio_vfo_offset(target - 200);
            s->kind = RADIO_FREQUENCY;
            s->min = 0;
        }
        break;
    case 2: case 3:
        s->label = field == 2 ? "RX SUB-AUDIO" : "TX SUB-AUDIO";
        s->offset += field == 2 ? 8 : 10; s->length = 2;
        s->choices = radio_tones; s->choice_count = sizeof radio_tones / sizeof *radio_tones; break;
    case 4:
        s->label = "BANDWIDTH"; s->offset += 14; s->mask = 8; s->shift = 3;
        s->choices = bandwidth; s->choice_count = 2; break;
    case 5:
        s->label = "TX POWER"; s->offset += 14; s->mask = 0x30; s->shift = 4;
        s->choices = power; s->choice_count = 3; break;
    case 6:
        s->label = "BUSY LOCK"; s->offset += 13; s->mask = 4; s->shift = 2;
        s->choices = onoff; s->choice_count = 2; break;
    case 7:
        s->label = "PTT-ID"; s->offset += 13; s->mask = 0xc0; s->shift = 6;
        s->choices = pttid; s->choice_count = 4; break;
    case 8:
        s->label = "SCRAMBLE"; s->offset += 12; s->kind = RADIO_NUMBER; s->max = 16; break;
    case 9:
        s->label = "HOPPING"; s->offset += 13; s->mask = 0x20; s->shift = 5;
        s->choices = onoff; s->choice_count = 2; break;
    case 10:
        s->label = "MODULATION"; s->offset += 15; s->mask = 1;
        s->choices = modulation; s->choice_count = 2; break;
    case 11:
        s->label = "OFFSET DIRECTION"; s->offset += 14; s->mask = 3;
        s->choices = direction; s->choice_count = 3; break;
    case 12:
        if (target > 199) return RADIO_INVALID;
        s->label = "SHORT NAME"; s->offset = 0xd40 + (target - 1) * 8;
        s->length = 8; s->kind = RADIO_TEXT; break;
    case 13:
    case 14:
        if (target > 199) return RADIO_INVALID;
        s->label = field == 13 ? "SCAN ADD" : "MEMORY ENABLE";
        s->offset = (field == 13 ? 0x1920 : 0x1900) + (target - 1) / 8;
        s->shift = (target - 1) % 8; s->mask = 1u << s->shift;
        s->choices = onoff; s->choice_count = 2; break;
    }
    if (field == 14) s->check_rx_offset = target * 16;
    return RADIO_OK;
}

static int set_flag(const struct radio_storage *storage, unsigned channel, unsigned field, unsigned value)
{
    struct radio_setting setting;
    struct radio_edit edit;
    int result = radio_channel_field(channel, field, &setting);
    if (!result) result = radio_begin(storage, &setting, &edit);
    if (!result) result = radio_set_number(&edit, value);
    if (!result) result = radio_save(storage, &edit);
    return result;
}

int radio_channel_copy(const struct radio_storage *storage, unsigned channel, unsigned vfo)
{
    if (!channel || channel > 199 || vfo > 1) return RADIO_INVALID;
    uint8_t record[16], check[16];
    if (storage->read(vfo ? 0x1960 : 0x1950, record, sizeof record)) return RADIO_IO_ERROR;
    struct radio_setting frequency;
    struct radio_edit edit;
    radio_channel_field(200 + vfo, 0, &frequency);
    edit.setting = &frequency;
    edit.channel_33cm = radio_33cm_enabled(storage);
    for (unsigned i = 0; i < 4; ++i) edit.value[i] = record[i];
    unsigned rx = radio_value(&edit);
    if (radio_set_number(&edit, rx)) return RADIO_INVALID;
    unsigned direction = record[14] & 3;
    if (direction) {
        if (direction > 2) return RADIO_INVALID;
        /* The native Store CHL handler resolves a VFO's repeater shift into
         * an actual TX frequency before copying its record to a memory. */
        if (storage->read(radio_vfo_offset(vfo), edit.value, 4)) return RADIO_IO_ERROR;
        unsigned shift = radio_value(&edit);
        if (shift > 60000000 || (direction == 1 && shift > rx)) return RADIO_INVALID;
        unsigned tx = direction == 1 ? rx - shift : rx + shift;
        if (radio_set_number(&edit, tx)) return RADIO_INVALID;
        for (unsigned i = 0; i < 4; ++i) record[4+i] = edit.value[i];
    }
    /* Keep an interrupted copy hidden. Publish the used flag only after the
     * complete record has been reread. An existing channel is hidden first. */
    int result = set_flag(storage, channel, 14, 0);
    if (result) return result;
    if (storage->write(channel * 16, record, sizeof record)) return RADIO_IO_ERROR;
    if (storage->read(channel * 16, check, sizeof check)) return RADIO_IO_ERROR;
    for (unsigned i = 0; i < sizeof record; ++i) if (record[i] != check[i]) return RADIO_VERIFY_ERROR;
    result = set_flag(storage, channel, 13, 1);
    return result ? result : set_flag(storage, channel, 14, 1);
}

int radio_channel_delete(const struct radio_storage *storage, unsigned channel)
{
    if (!channel || channel > 199) return RADIO_INVALID;
    int result = set_flag(storage, channel, 14, 0);
    if (!result) result = set_flag(storage, channel, 13, 0);
    return result;
}
