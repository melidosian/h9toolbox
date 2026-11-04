#ifndef H9_APRS_CLOCK_H
#define H9_APRS_CLOCK_H

#include <stdint.h>

/* Stock APRS 0x0800498C keeps the upper RCC_CFGR halfword, sets bit 13,
 * and inserts the low 13 factory bits. */
uint32_t h9_aprs_clock_cfgr_value(uint32_t previous, uint16_t factory);
void h9_aprs_clock_init(void);

#endif
