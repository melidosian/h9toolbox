/* Phone-style multi-tap text entry for APRS SEND (aprs_edit.c, linked into
 * SEND only). Digit d: 2 = ABC2 … 9 = WXYZ9, 0 = space 0, 1 = .,?!-/@1.
 * The same digit again within EDIT_CYCLE_TICKS changes the last character;
 * otherwise it adds one. Callsign fields take only A-Z, 0-9 and '-'. */
#ifndef APRS_EDIT_H
#define APRS_EDIT_H
#include <stdint.h>

enum { EDIT_MAX = 39, EDIT_CYCLE_TICKS = 100, EDIT_NO_DIGIT = 0xFF };

struct edit {
    char buf[EDIT_MAX + 1];
    uint8_t len, max, call, digit;
    uint32_t tick;
};

void edit_init(struct edit *e, unsigned max, int call, const char *text);
void edit_digit(struct edit *e, unsigned digit, uint32_t now);
void edit_back(struct edit *e);
#endif
