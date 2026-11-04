#ifndef H9_APRS_CLOCK_H
#define H9_APRS_CLOCK_H

#include <stdint.h>

/* Stock APRS 0x0800498C keeps the upper RCC_CFGR halfword, sets bit 13,
 * and inserts the low 13 factory bits. */
uint32_t h9_aprs_clock_cfgr_value(uint32_t previous, uint16_t factory);
void h9_aprs_clock_init(void);

/* Stock 0x08003B78 clock-source table and 0x080027B4 internal-oscillator
 * selection. A zero return means the oscillator or source did not become
 * ready within poll_limit reads. This is board setup after RAM init. */
uint32_t h9_aprs_clock_system_hz(uint32_t rcc_control,
                                 uint32_t rcc_trim,
                                 uint32_t rcc_select);
uint32_t h9_aprs_clock_select_internal(volatile uint32_t *rcc,
                                       uint16_t factory,
                                       uint32_t poll_limit);
uint32_t h9_aprs_clock_select_internal_target(uint32_t poll_limit);

#endif
