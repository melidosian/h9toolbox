#include "app_theme.h"
#include "radio_settings.h"

static const char *failure(int result)
{
    if (result == RADIO_INVALID) return "CHECK VALUE";
    if (result == RADIO_CONFLICT) return "CHANGED ELSEWHERE: REOPEN";
    if (result == RADIO_VERIFY_ERROR) return "SAVE READBACK FAILED";
    return "SETTINGS I/O FAILED";
}

static void draw_editor(const struct radio_edit *edit, unsigned selected,
                        unsigned editing, unsigned cursor, const char *notice)
{
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title(edit->setting->label);
    char buffer[RADIO_MAX_VALUE + 1];
    const char *value = radio_value_label(edit, buffer);
    gfx_text(3, 26, "VALUE", 5, 1, selected == 0 ? UI_ACCENT : UI_DIM, UI_BG);
    if (radio_is_text(edit->setting)) {
        for (unsigned i = 0; i < radio_text_length(edit->setting); ++i) {
            unsigned c = edit->value[i];
            char cell[] = { c >= 32 && c <= 126 ? c : ' ', 0 };
            unsigned active = editing && i == cursor;
            gfx_text(3 + (i % 25) * 6, 39 + (i / 25) * 10, cell, 1, 1,
                     active ? UI_BG : UI_TEXT, active ? UI_ACCENT : UI_BG);
        }
    } else gfx_text(3, 40, value, 26, 1, editing ? UI_ACCENT : UI_TEXT, UI_BG);
    gfx_text(3, 78, "SAVE", 4, 1, selected == 1 ? UI_ACCENT : UI_DIM, UI_BG);
    gfx_text(3, 92, "CANCEL", 6, 1, selected == 2 ? UI_ACCENT : UI_DIM, UI_BG);
    if (notice) gfx_text(3, 105, notice, 26, 1, UI_DANGER, UI_BG);
    ui_footer(editing ? "UP/DN EDIT MENU NEXT BACK" : "MENU PICK    BACK CANCEL");
}

int radio_editor(const struct radio_storage *storage, const struct radio_setting *setting)
{
    struct radio_edit edit;
    int result = radio_begin(storage, setting, &edit);
    if (result) return result;
    unsigned selected = 0, editing = 0, cursor = 0, number = 0, have_number = 0;
    const char *notice = 0;
    for (;;) {
        draw_editor(&edit, selected, editing, cursor, notice);
        unsigned key = gfx_wait_key();
        notice = 0;
        if (editing) {
            if (key == H9_KEY_BACK) { editing = 0; selected = 1; }
            else if (key == H9_KEY_MENU) {
                if (radio_is_text(setting)) cursor = (cursor + 1) % radio_text_length(setting);
                else { editing = 0; selected = 1; }
            } else if (radio_is_text(setting)) {
                unsigned c = edit.value[cursor];
                if (setting->kind >= RADIO_DTMF_FIXED && (key == H9_KEY_UP || key == H9_KEY_DOWN)) {
                    const char *tones = "0123456789ABCD*#";
                    unsigned i = 0; while (i < 16 && tones[i] != c) ++i;
                    i = key == H9_KEY_UP ? (i + 1) % 17 : (i + 16) % 17;
                    c = i < 16 ? tones[i] : 0;
                } else if (key == H9_KEY_UP) c = c < 32 ? 32 : c < 126 ? c + 1 : 0;
                else if (key == H9_KEY_DOWN) c = !c || c > 126 ? 126 : c == 32 ? 0 : c - 1;
                else if (key >= 2 && key <= 11) c = '0' + key - 2;
                else continue;
                edit.value[cursor] = c;
                if (!c) for (unsigned i = cursor; i < setting->length; ++i) edit.value[i] = 0;
                else for (unsigned i = 0; i < cursor; ++i) if (!edit.value[i] || edit.value[i] == 255)
                    edit.value[i] = setting->kind >= RADIO_DTMF_FIXED ? '0' : ' ';
                if (key >= 2 && key <= 11) cursor = (cursor + 1) % radio_text_length(setting);
            } else if (key == H9_KEY_UP || key == H9_KEY_DOWN) {
                radio_adjust(&edit, key == H9_KEY_UP ? 1 : -1);
                have_number = 0;
            } else if ((setting->kind == RADIO_NUMBER || setting->kind == RADIO_FREQUENCY) && key >= 2 && key <= 11) {
                unsigned candidate = (have_number ? number * 10 : 0) + key - 2;
                if (candidate <= setting->max) {
                    number = candidate;
                    have_number = 1;
                    result = radio_set_number(&edit, number);
                    if (result) notice = failure(result);
                } else notice = "VALUE TOO LARGE";
            }
        } else if (key == H9_KEY_BACK) return RADIO_OK;
        else if (key == H9_KEY_UP) selected = selected ? selected - 1 : 2;
        else if (key == H9_KEY_DOWN) selected = (selected + 1) % 3;
        else if (key == H9_KEY_MENU) {
            if (selected == 2) return RADIO_OK;
            if (!selected) { editing = 1; have_number = 0; }
            else {
                result = radio_save(storage, &edit);
                if (result) notice = failure(result);
                else return -1; /* saved; caller applies the radio configuration */
            }
        }
    }
}
