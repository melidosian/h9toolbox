#include "app_theme.h"
#include "key_ramp.h"
#include "radio_channels.h"
#include "channel_labels_native.h"

enum { LONG_NAME_ROW = 14, MEMORY_FIELDS = RADIO_CHANNEL_FIELDS + 1, LONG_NAME_BYTES = 32, MEMORIES = 199 };

static const struct radio_storage *channel_editor_field(const struct radio_storage *stock,
    unsigned target, unsigned row, struct radio_setting *setting)
{
    if (row == LONG_NAME_ROW) {
        *setting = (struct radio_setting){ .id = "longName", .label = "LONG NAME",
            .kind = RADIO_TEXT, .offset = target, .length = LONG_NAME_BYTES };
        return &radio_long_name_storage;
    }
    unsigned index = row - 1;
    if (row > LONG_NAME_ROW) --index;
    radio_channel_field(target, index, setting);
    return stock;
}

static int clear_label(unsigned target)
{
    char value[LONG_NAME_BYTES];
    int result = radio_long_name_storage.read(target, value, sizeof value);
    if (result || !value[0]) return result;
    const char empty[LONG_NAME_BYTES] = {0};
    return radio_long_name_storage.write(target, empty, sizeof empty);
}

static void channel_number(unsigned channel, char text[4])
{
    text[0] = '0' + channel / 100;
    text[1] = '0' + channel / 10 % 10;
    text[2] = '0' + channel % 10;
    text[3] = 0;
}

#ifndef CHANNEL_TICK
#define CHANNEL_TICK (*(const volatile uint32_t *)0x1D8B0u)
#endif

static unsigned channel_key(struct h9_key_ramp *input, unsigned scroll)
{
    unsigned key;
    do {
        h9_delay(20);
        key = h9_key_ramp(input, h9_key(), CHANNEL_TICK, scroll);
    } while (!key);
    return key;
}

int radio_channel_editor(const struct radio_storage *storage, uint16_t accent)
{
    unsigned target = 0, selected = 1, changed = 0, confirm = 0;
    unsigned held = h9_key();
    struct h9_key_ramp input = { .sample = held, .stable = held };
    uint16_t ink = menu_ink(accent);
    unsigned editing = 0, number = 0, have_number = 0, number_invalid = 0;
    struct radio_setting active_setting;
    struct radio_edit draft;
    const char *notice = 0;
    unsigned draw_frame = 1;
    for (;;) {
        if (target || draw_frame) {
            gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
            gfx_text(UI_LEFT, UI_TITLE_Y, "CHANNEL SETTINGS", UI_CELLS, 1, ink, UI_BG);
            char label[4];
            channel_number(target, label);
            gfx_text(3, 24, target ? label : "SELECT CHANNEL", 16, 1, ink, UI_BG);
            const char *footer = editing && active_setting.kind == RADIO_CHOICE ? "UP/DN   MENU SAVE   BACK" :
                editing ? "0-9  MENU SAVE    BACK" : !target ? "0-9 JUMP  MENU OPEN  BACK" :
                confirm ? "MENU CONFIRM  BACK CANCEL" : "MENU EDIT     BACK RETURN";
            gfx_text(UI_LEFT, UI_FOOT_Y, footer, UI_CELLS, 1, UI_TEXT, UI_BG);
        }
        draw_frame = target;
        unsigned count = target ? MEMORY_FIELDS + 3 : MEMORIES;
        unsigned top = selected < 3 ? 1 : selected - 2;
        if (top + 4 > count) top = count - 4;
        for (unsigned row = 0; row < 5; ++row) {
            unsigned field = top + row;
            struct radio_setting setting;
            struct radio_edit current;
            const char *text;
            const char *value = ">";
            char buffer[RADIO_MAX_VALUE + 1];
            char name[4];
            if (!target) {
                channel_number(field, name);
                text = name;
                buffer[LONG_NAME_BYTES] = 0;
                if (!radio_long_name_storage.read(field, buffer, LONG_NAME_BYTES) && buffer[0]) value = buffer;
                else {
                    radio_channel_field(field, 12, &setting);
                    value = radio_begin(storage, &setting, &current) ? "" : radio_value_label(&current, buffer);
                }
            } else if (field <= MEMORY_FIELDS) {
                const struct radio_storage *source = channel_editor_field(storage, target, field, &setting);
                text = setting.label;
                if (editing && field == selected) value = have_number && number_invalid ? radio_number_label(number, buffer)
                    : radio_value_label(&draft, buffer);
                else value = radio_begin(source, &setting, &current) == RADIO_OK
                    ? radio_value_label(&current, buffer) : "READ ERROR";
            } else text = field == MEMORY_FIELDS + 1 ? "STORE VFO A" :
                          field == MEMORY_FIELDS + 2 ? "STORE VFO B" : "DELETE CHANNEL";
            unsigned active = selected == field;
            uint16_t bg = active ? (editing ? UI_EDIT : accent) : UI_BG;
            uint16_t fg = active && menu_luma(bg) >= 820 ? UI_BG : UI_TEXT;
            gfx_fill(0, 40 + row * 12, H9_WIDTH, 12, bg);
            unsigned n = 0;
            while (n < 10 && value[n]) ++n;
            gfx_text(3, 42 + row * 12, text, target ? 24 - n : 3, 1, fg, bg);
            gfx_text(target ? 157 - n * 6 : 33, 42 + row * 12, value, target ? n : 21,
                     1, active ? fg : ink, bg);
        }
        if (notice) gfx_text(3, 105, notice, 26, 1, ink, UI_BG);
        unsigned key = channel_key(&input, !editing && !confirm);
        if (!target && notice) gfx_fill(0, 105, H9_WIDTH, 8, UI_BG);
        notice = 0;
        if (editing) {
            if (key == H9_KEY_BACK) editing = 0;
            else if (key == H9_KEY_UP || key == H9_KEY_DOWN) {
                radio_adjust(&draft, key == H9_KEY_UP ? 1 : -1);
                have_number = number_invalid = 0;
            } else if (key >= 2 && key <= 11 && active_setting.kind != RADIO_CHOICE) {
                unsigned value = (have_number ? number * 10 : 0) + key - 2;
                if (have_number == 8) {
                    notice = "VALUE OUT OF RANGE"; number_invalid = 1; have_number = 0;
                } else {
                    number = value;
                    ++have_number;
                    if (radio_is_frequency(&active_setting))
                        for (unsigned digits = have_number; digits < 8; ++digits) value *= 10;
                    number_invalid = (radio_is_frequency(&active_setting) && have_number < 3) ||
                        radio_set_number(&draft, value) != RADIO_OK;
                }
            } else if (key == H9_KEY_MENU) {
                int result = number_invalid ? RADIO_INVALID : radio_save(storage, &draft);
                if (!result) { changed = 1; editing = 0; notice = "SAVED"; }
                else notice = result == RADIO_INVALID ? "CHECK VALUE" : "SAVE FAILED";
            }
        } else if (confirm) {
            if (key == H9_KEY_BACK) confirm = 0;
            else if (key == H9_KEY_MENU) {
                unsigned deleting = selected == MEMORY_FIELDS + 3;
                int result = deleting ? radio_channel_delete(storage, target)
                    : radio_channel_copy(storage, target, selected - MEMORY_FIELDS - 1);
                confirm = 0;
                if (!result) {
                    changed = 1;
                    notice = deleting && clear_label(target) ? "CHANNEL OFF; NAME KEPT" : "SAVED";
                } else notice = "SAVE FAILED";
            }
        } else if (!target && key >= 2 && key <= 11) {
            if (have_number == 3) number = have_number = 0;
            number = number * 10 + key - 2;
            ++have_number;
            number_invalid = !number || number > MEMORIES;
            if (!number_invalid) selected = number;
            else if (number) notice = "CHANNEL 001-199";
        } else if (key == H9_KEY_BACK) {
            if (!target) return changed ? -1 : RADIO_OK;
            selected = target;
            target = 0;
            number = have_number = number_invalid = 0;
        } else if (key == H9_KEY_UP || key == H9_KEY_DOWN) {
            selected = key == H9_KEY_UP ? (selected > 1 ? selected - 1 : count) :
                (selected < count ? selected + 1 : 1);
            number = have_number = number_invalid = 0;
        } else if (key == H9_KEY_MENU) {
            if (!target) {
                if (number_invalid) notice = "CHANNEL 001-199";
                else {
                    target = selected;
                    selected = 1;
                }
            } else if (selected > MEMORY_FIELDS) { confirm = 1; notice = "CHANGE THIS MEMORY?"; }
            else {
                const struct radio_storage *source = channel_editor_field(storage, target, selected, &active_setting);
                if (active_setting.kind == RADIO_TEXT) {
                    int result = radio_editor(source, &active_setting);
                    held = h9_key();
                    input = (struct h9_key_ramp){ .sample = held, .stable = held };
                    if (result == -1) { changed = 1; notice = "SAVED"; }
                    else if (result) notice = "READ ERROR";
                } else {
                    int result = radio_begin(storage, &active_setting, &draft);
                    if (!result) { editing = 1; have_number = number_invalid = 0; }
                    else notice = "READ ERROR";
                }
            }
        }
    }
}
