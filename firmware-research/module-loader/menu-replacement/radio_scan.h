#ifndef RADIO_SCAN_H
#define RADIO_SCAN_H
#include <stdint.h>

enum radio_scan_mode {
    RADIO_SCAN_TONE, RADIO_SCAN_FREQUENCY, RADIO_SCAN_RANGER, RADIO_SCAN_SPECTRUM
};
/* Mode RANGER uses the saved lower/upper bounds. The native Freq Ranger row
 * itself is represented by those two inline settings in the unified menu. */
int radio_scan(unsigned mode); /* 0 cancelled, -1 saved to VFO, positive I/O error */

/* Decoded values use the channel codec: CTCSS packed BCD, DCS 0x8000/0xc000
 * plus three octal digits packed into BCD. 0xffff means no valid code.
 * Cyclic DCS aliases return a canonical equivalent code so changing detector
 * phase cannot prevent the two-observation lock from settling. */
unsigned radio_scan_ctcss(unsigned reg68);
unsigned radio_scan_dcs(unsigned reg69, unsigned reg6a);
unsigned radio_scan_dcs_word(unsigned wire_code);
unsigned radio_scan_counter(unsigned reg0d, unsigned reg0e);
uint32_t radio_scan_next(uint32_t frequency, uint32_t low, uint32_t high, uint32_t step, int direction);
#endif
