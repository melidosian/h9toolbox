#ifndef H9_MAIN_TX_FREQUENCY_H
#define H9_MAIN_TX_FREQUENCY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t region_mode;
    bool allow_200_to_300_mhz;
    bool allow_300_to_400_mhz;
    bool allow_500_mhz_plus;
} H9MainTxFrequencyPolicy;

/* Stock app+0x0569E2 applies repeater offset only when simplex is false.
 * Mode 1 subtracts, mode 2 adds; mode 0 keeps the base frequency.
 * Inputs and output use 10 Hz units. Invalid modes/overflow are rejected
 * as replacement safeguards. */
bool h9_main_tx_apply_shift(uint32_t base_10hz, bool simplex,
                             uint8_t shift_mode, uint32_t offset_10hz,
                             uint32_t *tx_10hz);

/* Ports the band, region, and 30-frequency table precheck in stock
 * app+0x05ADCA..0x05AE60. The later FUN_01e56712 check is not ported. */
bool h9_main_tx_frequency_allowed(const H9MainTxFrequencyPolicy *policy,
                                   uint32_t tx_10hz);

/* The TX service and clone settings use Hz; the stock RF path uses 10 Hz. */
bool h9_main_tx_hz_allowed(const H9MainTxFrequencyPolicy *policy,
                            uint32_t tx_hz);

#endif
