#define H9_KEY_RAMP_IMPLEMENTATION
#include "key_ramp.h"
#include "radio_settings.h"

static void copy(uint8_t *dst, const uint8_t *src, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) dst[i] = src[i];
}

static unsigned frequency_value(const uint8_t *bytes)
{
        unsigned value = 0, place = 1;
        for (unsigned i = 0; i < 4; ++i) {
            unsigned lo = bytes[i] & 15, hi = bytes[i] >> 4;
            if (lo > 9 || hi > 9) return ~0u;
            value += (lo + 10 * hi) * place;
            place *= 100;
        }
        return value;
}

unsigned radio_value(const struct radio_edit *edit)
{
    const struct radio_setting *s = edit->setting;
    if (s->kind == RADIO_FREQUENCY) return frequency_value(edit->value);
    if (s->kind == RADIO_SIGNED16) return (int16_t)(edit->value[0] | (unsigned)edit->value[1] << 8);
    if (s->kind == RADIO_DEGREES || s->kind == RADIO_ALTITUDE) {
        unsigned n = 0;
        for (unsigned i = 0; i < 4; ++i) n = n << 8 | edit->value[i];
        return s->kind == RADIO_ALTITUDE ? (int32_t)n / 1000000 : n;
    }
    unsigned n = edit->value[0];
    if (s->length == 2 && s->kind != RADIO_TEXT) return n | (unsigned)edit->value[1] << 8;
    return (n & s->mask) >> s->shift;
}

static int valid_number(const struct radio_setting *s, unsigned n)
{
    if (s->kind == RADIO_CHOICE) {
        for (unsigned i = 0; i < s->choice_count; ++i) if (s->choices[i].value == n) return 1;
        return 0;
    }
    if (radio_is_signed(s)) return (int32_t)n >= s->min && (int32_t)n <= s->max;
    return n >= (unsigned)s->min && n <= (unsigned)s->max;
}

static void set_number(struct radio_edit *edit, unsigned n)
{
    const struct radio_setting *s = edit->setting;
    if (s->kind == RADIO_FREQUENCY) {
        for (unsigned i = 0; i < 4; ++i) {
            unsigned pair = n % 100;
            edit->value[i] = pair % 10 | (pair / 10) << 4;
            n /= 100;
        }
    } else if (s->kind == RADIO_DEGREES || s->kind == RADIO_ALTITUDE) {
        if (s->kind == RADIO_ALTITUDE) n = (int32_t)n * 1000000;
        for (unsigned i = 4; i; --i) { edit->value[i - 1] = n; n >>= 8; }
    } else if (s->length == 2) { edit->value[0] = n; edit->value[1] = n >> 8; }
    else edit->value[0] = (edit->value[0] & ~s->mask) | ((n << s->shift) & s->mask);
}

int radio_set_number(struct radio_edit *edit, unsigned n)
{
    if (radio_is_text(edit->setting) || !valid_number(edit->setting, n)) return RADIO_INVALID;
    set_number(edit, n);
    return RADIO_OK;
}

int radio_begin(const struct radio_storage *storage, const struct radio_setting *s, struct radio_edit *edit)
{
    if (!s || !s->length || s->length > RADIO_MAX_VALUE) return RADIO_INVALID;
    edit->setting = s;
    if (storage->read(s->offset, edit->original, s->length)) return RADIO_IO_ERROR;
    copy(edit->value, edit->original, s->length);
    if (s->kind >= RADIO_DTMF_FIXED) {
        static const char tones[] = "0123456789ABCD*#";
        unsigned start = s->kind == RADIO_DTMF_LENGTH, count = radio_text_length(s);
        if (start && edit->original[0] < count) count = edit->original[0];
        unsigned ended = start && edit->original[0] > radio_text_length(s);
        for (unsigned i = 0; i < s->length; ++i) {
            unsigned c = i < count ? edit->original[start + i] : 255;
            ended |= c == 255 || i >= count;
            edit->value[i] = ended ? 0 : c < 16 ? tones[c] : '?';
        }
    } else if (s->kind == RADIO_TEXT) {
        unsigned ended = 0;
        for (unsigned i = 0; i < s->length; ++i) {
            ended |= edit->value[i] == 0 || edit->value[i] == 255;
            if (ended) edit->value[i] = 0;
        }
    }
    return RADIO_OK;
}

int radio_adjust(struct radio_edit *edit, int direction)
{
    const struct radio_setting *s = edit->setting;
    if (radio_is_text(s) || !direction) return RADIO_INVALID;
    unsigned n = radio_value(edit);
    if (s->kind == RADIO_CHOICE) {
        if (!s->choice_count) return RADIO_INVALID;
        unsigned i = 0;
        while (i < s->choice_count && s->choices[i].value != n) ++i;
        if (i == s->choice_count) i = direction > 0 ? 0 : s->choice_count - 1;
        else i = direction > 0 ? (i + 1) % s->choice_count : (i ? i - 1 : s->choice_count - 1);
        n = s->choices[i].value;
    } else if (!valid_number(s, n)) n = direction > 0 ? s->min : s->max;
    else if (radio_is_signed(s)) {
        int32_t value = (int32_t)n;
        int32_t step = s->kind == RADIO_SIGNED16 ? 10 : 1;
        n = direction > 0 ? (value <= s->max - step ? value + step : s->min)
            : (value >= s->min + step ? value - step : s->max);
    }
    else {
        unsigned step = s->kind == RADIO_FREQUENCY ? 500 : 1; /* 5 kHz or one displayed unit */
        n = direction > 0 ? (n <= s->max - step ? n + step : s->min) : (n >= s->min + step ? n - step : s->max);
    }
    set_number(edit, n);
    return RADIO_OK;
}

int radio_set_text(struct radio_edit *edit, const char *text, unsigned length)
{
    const struct radio_setting *s = edit->setting;
    if (!radio_is_text(s) || length > radio_text_length(s)) return RADIO_INVALID;
    for (unsigned i = 0; i < length; ++i) if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126) return RADIO_INVALID;
    if (s->kind >= RADIO_DTMF_FIXED) {
        static const char tones[] = "0123456789ABCD*#";
        for (unsigned i = 0; i < length; ++i) {
            unsigned tone = 0;
            while (tone < 16 && tones[tone] != text[i]) ++tone;
            if (tone == 16) return RADIO_INVALID;
        }
    }
    if (s->id[0] == 'c' && s->id[1] == 'a' && s->id[2] == 'l' && s->id[3] == 'l' && !s->id[4]) {
        if (!length) return RADIO_INVALID;
        for (unsigned i = 0; i < length; ++i)
            if (!((text[i] >= 'A' && text[i] <= 'Z') || (text[i] >= 'a' && text[i] <= 'z') ||
                  (text[i] >= '0' && text[i] <= '9'))) return RADIO_INVALID;
    }
    for (unsigned i = 0; i < s->length; ++i) edit->value[i] = i < length ? text[i] : 0;
    return RADIO_OK;
}

int radio_save(const struct radio_storage *storage, struct radio_edit *edit)
{
    const struct radio_setting *s = edit->setting;
    uint8_t now[RADIO_MAX_VALUE], check[RADIO_MAX_VALUE];
    if (!radio_is_text(s) && !valid_number(s, radio_value(edit))) return RADIO_INVALID;
    if (s->check_rx_offset && radio_value(edit)) {
        if (storage->read(s->check_rx_offset, check, 4)) return RADIO_IO_ERROR;
        unsigned frequency = frequency_value(check);
        if (frequency < 1800000 || frequency > 60000000) return RADIO_INVALID;
    }
    if (radio_is_text(s)) {
        unsigned ended = 0;
        for (unsigned i = 0; i < s->length; ++i) {
            unsigned c = edit->value[i];
            if (!c) ended = 1;
            else if (ended || c < 32 || c > 126) return RADIO_INVALID;
        }
        unsigned length = 0;
        while (length < s->length && edit->value[length]) ++length;
        if (radio_set_text(edit, (const char *)edit->value, length) != RADIO_OK) return RADIO_INVALID;
    }
    uint8_t encoded[RADIO_MAX_VALUE];
    const uint8_t *next = edit->value;
    if (s->kind >= RADIO_DTMF_FIXED) {
        static const char tones[] = "0123456789ABCD*#";
        for (unsigned i = 0; i < s->length; ++i) encoded[i] = 255;
        unsigned length = 0, start = s->kind == RADIO_DTMF_LENGTH;
        while (length < radio_text_length(s) && edit->value[length]) {
            unsigned tone = 0;
            while (tone < 16 && tones[tone] != edit->value[length]) ++tone;
            encoded[start + length++] = tone;
        }
        if (start) encoded[0] = length;
        next = encoded;
    }
    if (storage->read(s->offset, now, s->length)) return RADIO_IO_ERROR;
    unsigned changed = 0;
    for (unsigned i = 0; i < s->length; ++i) {
        unsigned mask = s->length == 1 && !radio_is_text(s) ? s->mask : 255;
        if ((now[i] & mask) != (edit->original[i] & mask)) return RADIO_CONFLICT;
        changed |= (now[i] ^ next[i]) & mask;
        now[i] = (now[i] & ~mask) | (next[i] & mask);
    }
    if (changed) {
        if (storage->write(s->offset, now, s->length)) return RADIO_IO_ERROR;
        if (storage->read(s->offset, check, s->length)) return RADIO_IO_ERROR;
        for (unsigned i = 0; i < s->length; ++i) if (check[i] != now[i]) return RADIO_VERIFY_ERROR;
    }
    copy(edit->original, now, s->length);
    if (s->kind < RADIO_DTMF_FIXED) copy(edit->value, now, s->length);
    return RADIO_OK;
}

const char *radio_frequency_label(unsigned f, char text[10])
{
    if (f > 99999999) return "--";
    for (unsigned i = 9, place = 1; i;) {
        --i;
        if (i == 3) text[i] = '.';
        else { text[i] = '0' + f / place % 10; place *= 10; }
    }
    unsigned end = 9;
    while (end > 7 && text[end - 1] == '0') --end;
    text[end] = 0;
    return text;
}

const char *radio_value_label(const struct radio_edit *edit, char buffer[RADIO_MAX_VALUE + 1])
{
    const struct radio_setting *s = edit->setting;
    if (radio_is_text(s)) {
        unsigned i = 0;
        while (i < s->length && edit->value[i] && edit->value[i] != 255) {
            unsigned c = edit->value[i];
            buffer[i++] = c >= 32 && c <= 126 ? c : '?';
        }
        buffer[i] = 0;
        return buffer;
    }
    unsigned n = radio_value(edit);
    if (s->kind == RADIO_FREQUENCY) {
        if (!valid_number(s, n)) return "UNSET";
        if (s->min) return radio_frequency_label(n, buffer);
    }
    if (s->kind == RADIO_CHOICE) {
        for (unsigned i = 0; i < s->choice_count; ++i) if (s->choices[i].value == n) return s->choices[i].label;
        return "UNSET";
    }
    if (radio_is_signed(s) && (int32_t)n < 0) {
        buffer[0] = '-';
        radio_number_label(-(int32_t)n, buffer + 1);
        return buffer;
    }
    if (s->kind == RADIO_DEGREES) {
        unsigned integer = n / 1000000, fraction = n % 1000000;
        radio_number_label(integer, buffer);
        unsigned i = 0; while (buffer[i]) ++i;
        buffer[i++] = '.';
        for (unsigned place = 100000; place; place /= 10) { buffer[i++] = '0' + fraction / place; fraction %= place; }
        buffer[i] = 0;
        return buffer;
    }
    return radio_number_label(n, buffer);
}

const char *radio_number_label(unsigned n, char *buffer)
{
    char digits[10];
    unsigned i = 0, j = 0;
    do { digits[i++] = '0' + n % 10; n /= 10; } while (n);
    while (i) buffer[j++] = digits[--i];
    buffer[j] = 0;
    return buffer;
}

__attribute__((noinline)) unsigned menu_luma(uint16_t rgb)
{
    return 26 * (rgb >> 11) + 26 * ((rgb >> 5) & 63) + 10 * (rgb & 31);
}

uint16_t menu_ink(uint16_t accent)
{
    /* Lighten dark colors for small text on the black LCD background. */
    return menu_luma(accent) < 1050 ? ((accent >> 1) & 0x7BEF) + 0x8410 : accent;
}
