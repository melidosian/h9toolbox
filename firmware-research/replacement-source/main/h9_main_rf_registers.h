#ifndef H9_MAIN_RF_REGISTERS_H
#define H9_MAIN_RF_REGISTERS_H

#include <stdbool.h>
#include <stdint.h>

/* The stock routine at app+0x058F94 takes frequency in 10 Hz units.
 * Band 0 uses the signed halfword at RAM 0x12138, band 1 uses 0x12136;
 * other band values skip correction. The correction is divided by 10
 * before addition. The caller supplies captured/calibrated halfwords. */
bool h9_main_rf_frequency_words(uint32_t frequency_10hz,
                                uint8_t band_selector,
                                int16_t lower_trim,
                                int16_t upper_trim,
                                uint16_t *low_word,
                                uint16_t *high_word);

/* Stock app+0x05914C maps a mode to RF register 0x75 (RX path 0) or
 * 0x74 (TX path 1). This is only the register value; it does not key RF. */
bool h9_main_rf_mode_word(uint8_t path, uint32_t mode,
                          uint8_t *register_number, uint16_t *word);

/* Stock app+0x0592E4 sends 0x6040 | (mode << 8) to RF register 0x47.
 * The effect on the APRS ADC and Bluetooth paths is not established. */
bool h9_main_rf_af_word(uint8_t mode, uint16_t *word);

typedef bool (*H9MainRfWrite)(void *opaque, uint8_t register_number,
                               uint16_t word);

/* Port of the first two RF-register writes at app+0x058FCE/0x058FD6.
 * Later stock setup has unported side effects and must not be assumed
 * safe for use as an unkeyed TX preparation adapter. */
bool h9_main_rf_write_frequency(uint32_t frequency_10hz,
                                 uint8_t band_selector,
                                 int16_t lower_trim,
                                 int16_t upper_trim,
                                 H9MainRfWrite write_register,
                                 void *opaque);

bool h9_main_rf_write_af(uint8_t mode, H9MainRfWrite write_register,
                         void *opaque);

#endif
