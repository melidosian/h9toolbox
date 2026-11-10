/* Multi-tap text entry for APRS SEND; see aprs_edit.h. */
#include "aprs_edit.h"

static const char *const KEYS[10] = {
    " 0", ".,?!-/@1", "ABC2", "DEF3", "GHI4", "JKL5", "MNO6", "PQRS7", "TUV8", "WXYZ9",
};

static int allowed(char c, int call)
{
    return !call || c == '-' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z');
}

void edit_init(struct edit *e, unsigned max, int call, const char *text)
{
    e->max = (uint8_t)(max < EDIT_MAX ? max : EDIT_MAX);
    e->call = (uint8_t)call;
    e->digit = EDIT_NO_DIGIT;
    e->len = 0;
    while (text && text[e->len] && e->len < e->max) {
        e->buf[e->len] = text[e->len];
        e->len++;
    }
    e->buf[e->len] = 0;
}

void edit_digit(struct edit *e, unsigned digit, uint32_t now)
{
    if (digit > 9) return;
    const char *set = KEYS[digit];
    unsigned size = 0, start = 0, n = e->len;
    while (set[size]) size++;
    if (n && digit == e->digit && (uint32_t)(now - e->tick) < EDIT_CYCLE_TICKS) {
        while (start < size && set[start] != e->buf[n - 1]) start++;
        start++;                                    /* the character after the current one */
        n--;
    } else if (n >= e->max) {
        return;                                     /* full */
    }
    for (unsigned k = 0; k < size; k++) {
        char c = set[(start + k) % size];
        if (!allowed(c, e->call)) continue;
        e->buf[n] = c;
        e->buf[n + 1] = 0;
        e->len = (uint8_t)(n + 1);
        e->digit = (uint8_t)digit;
        e->tick = now;
        return;
    }
}

void edit_back(struct edit *e)
{
    if (e->len) e->buf[--e->len] = 0;
    e->digit = EDIT_NO_DIGIT;
}
