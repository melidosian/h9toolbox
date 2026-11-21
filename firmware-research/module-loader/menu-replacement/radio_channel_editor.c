#include "app_theme.h"
#include "radio_channels.h"
#include "channel_labels_native.h"

enum { LONG_NAME_ROW = 14, MEMORY_FIELDS = RADIO_CHANNEL_FIELDS + 1, LONG_NAME_BYTES = 32 };

static const struct radio_storage *channel_editor_field(const struct radio_storage *stock,
    unsigned target, unsigned row, struct radio_setting *setting)
{
    if (target <= 199 && row == LONG_NAME_ROW) {
        *setting = (struct radio_setting){ .id = "longName", .label = "LONG NAME",
            .kind = RADIO_TEXT, .offset = target, .length = LONG_NAME_BYTES };
        return &radio_long_name_storage;
    }
    unsigned index = row - 1;
    if (target <= 199 && row > LONG_NAME_ROW) --index;
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

static void target_label(unsigned target, char label[16])
{
    if (target > 199) {
        const char *vfo = target == 200 ? "VFO A" : "VFO B";
        for (unsigned i = 0; i < 6; ++i) label[i] = vfo[i];
    } else {
        const char *prefix = "CHANNEL ";
        for (unsigned i = 0; i < 8; ++i) label[i] = prefix[i];
        label[8] = '0' + target / 100; label[9] = '0' + target / 10 % 10;
        label[10] = '0' + target % 10; label[11] = 0;
    }
}

int radio_channel_editor(const struct radio_storage *storage)
{
    unsigned target = 1, selected = 0, choose_target = 0, changed = 0, confirm = 0;
    unsigned original_target = 1;
    unsigned editing = 0, number = 0, have_number = 0, number_invalid = 0;
    struct radio_setting active_setting;
    struct radio_edit draft;
    const char *notice = 0;
    for (;;) {
        gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
        ui_title("CHANNEL PROGRAMMING");
        char label[16];
        target_label(target, label);
        gfx_text(3, 24, label, 16, 1, choose_target ? UI_EDIT : selected == 0 ? UI_ACCENT : UI_TEXT, UI_BG);
        unsigned fields = target <= 199 ? MEMORY_FIELDS : 12;
        unsigned count = target <= 199 ? fields + 3 : fields;
        unsigned top = selected < 3 ? 1 : selected - 2;
        if (top + 5 > count + 1) top = count > 4 ? count - 4 : 1;
        for (unsigned row = 0; row < 5; ++row) {
            unsigned field = top + row;
            if (field > count) break;
            struct radio_setting setting;
            const char *text;
            const char *value = ">";
            char buffer[RADIO_MAX_VALUE + 1];
            if (field <= fields) {
                const struct radio_storage *source = channel_editor_field(storage, target, field, &setting);
                text = setting.label;
                if (editing && field == selected) value = have_number && number_invalid ? radio_number_label(number, buffer)
                    : radio_value_label(&draft, buffer);
                else {
                    struct radio_edit current;
                    value = radio_begin(source, &setting, &current) == RADIO_OK
                        ? radio_value_label(&current, buffer) : "READ ERROR";
                }
            } else text = field == fields + 1 ? "STORE VFO A" :
                          field == fields + 2 ? "STORE VFO B" : "DELETE CHANNEL";
            unsigned n = 0;
            while (value[n] && n < 10) ++n;
            uint16_t bg = selected == field ? (editing ? UI_EDIT : UI_ACCENT) : UI_BG;
            gfx_fill(0, 40 + row * 12, H9_WIDTH, 12, bg);
            gfx_text(3, 42 + row * 12, text, 24 - n, 1, selected == field ? UI_BG : UI_TEXT, bg);
            gfx_text(157 - n * 6, 42 + row * 12, value, n, 1, selected == field ? UI_BG : UI_ACCENT, bg);
        }
        if (notice) gfx_text(3, 105, notice, 26, 1, UI_HINT, UI_BG);
        ui_footer(editing ? "0-9 VALUE MENU SAVE BACK" : confirm ? "MENU CONFIRM  BACK CANCEL" :
                  choose_target ? "0-9 TARGET MENU SAVE BACK" : "MENU EDIT     BACK RETURN");
        unsigned key = gfx_wait_key();
        notice = 0;
        if (editing) {
            if (key == H9_KEY_BACK) editing = 0;
            else if (key == H9_KEY_UP || key == H9_KEY_DOWN) {
                radio_adjust(&draft, key == H9_KEY_UP ? 1 : -1);
                have_number = number_invalid = 0;
            } else if (key >= 2 && key <= 11) {
                unsigned value = active_setting.kind == RADIO_CHOICE ? key - 2 :
                    (have_number ? number * 10 : 0) + key - 2;
                if (value > 99999999) {
                    notice = "VALUE OUT OF RANGE"; number_invalid = 1; have_number = 0;
                } else {
                    number = value; have_number = 1;
                    number_invalid = radio_set_number(&draft, value) != RADIO_OK;
                    if (number_invalid) notice = "VALUE OUT OF RANGE";
                }
            } else if (key == H9_KEY_MENU) {
                int result = number_invalid ? RADIO_INVALID : radio_save(storage, &draft);
                if (!result) { changed = 1; editing = 0; notice = "SAVED"; }
                else notice = result == RADIO_INVALID ? "CHECK VALUE" : "CHANNEL SAVE FAILED";
            }
        } else if (confirm) {
            if (key == H9_KEY_BACK) confirm = 0;
            else if (key == H9_KEY_MENU) {
                unsigned deleting = selected == fields + 3;
                int result = deleting ? radio_channel_delete(storage, target)
                    : radio_channel_copy(storage, target, selected - fields - 1);
                confirm = 0;
                if (!result) {
                    changed = 1;
                    notice = deleting && clear_label(target) ? "CHANNEL OFF; NAME KEPT" : "SAVED";
                } else notice = "CHANNEL SAVE FAILED";
            }
        } else if (choose_target) {
            if (key == H9_KEY_UP || key == H9_KEY_DOWN) {
                target = key == H9_KEY_UP ? (target < RADIO_CHANNEL_TARGETS ? target + 1 : 1)
                    : (target > 1 ? target - 1 : RADIO_CHANNEL_TARGETS);
                have_number = number_invalid = 0;
            } else if (key >= 2 && key <= 11) {
                number = (have_number ? number * 10 : 0) + key - 2;
                have_number = 1;
                number_invalid = !number || number > RADIO_CHANNEL_TARGETS;
                if (number_invalid) { notice = "1-199 CH  200 A  201 B"; if (number > RADIO_CHANNEL_TARGETS) have_number = 0; }
                else target = number;
            } else if (key == H9_KEY_BACK) { target = original_target; choose_target = 0; }
            else if (key == H9_KEY_MENU) {
                if (number_invalid) notice = "1-199 CH  200 A  201 B";
                else choose_target = 0;
            }
        } else if (key == H9_KEY_BACK) return changed ? -1 : RADIO_OK;
        else if (key == H9_KEY_UP) selected = selected ? selected - 1 : count;
        else if (key == H9_KEY_DOWN) selected = selected < count ? selected + 1 : 0;
        else if (key == H9_KEY_MENU) {
            if (!selected) {
                choose_target = 1; original_target = target;
                number = have_number = number_invalid = 0;
            }
            else if (selected > fields) { confirm = 1; notice = "CHANGE THIS MEMORY?"; }
            else {
                const struct radio_storage *source = channel_editor_field(storage, target, selected, &active_setting);
                if (active_setting.kind == RADIO_TEXT) {
                    int result = radio_editor(source, &active_setting);
                    if (result == -1) { changed = 1; notice = "SAVED"; }
                    else if (result) notice = "CHANNEL READ FAILED";
                } else {
                    int result = radio_begin(storage, &active_setting, &draft);
                    if (!result) { editing = 1; have_number = number_invalid = 0; }
                    else notice = "CHANNEL READ FAILED";
                }
            }
        }
    }
}
