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

/* Stock RAM 0xC120/0xC124 form range 0 (used by TX setup), while
 * 0xC118/0xC11C form range 1. Values are supplied at runtime. */
typedef struct {
    uint32_t range0_low_10hz;
    uint32_t range0_high_10hz;
    uint32_t range1_low_10hz;
    uint32_t range1_high_10hz;
} H9MainTxLimits;

/* Initial words copied from stock app+0x0D22A4..0x0D22B0 into RAM
 * 0xC118..0xC124. */
extern const H9MainTxLimits h9_main_stock_tx_limits;

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

/* Port of stock app+0x056612 for range selectors 0 and 1. On failure it
 * writes the opposite boundary, as stock does, and returns false. The
 * TX call uses the frequency-derived range with normalization disabled. */
bool h9_main_tx_limit_check(uint32_t *frequency_10hz,
                             const H9MainTxLimits *limits,
                             uint8_t range_selector,
                             bool normalize_to_lower,
                             bool allow_200_to_300_mhz);

/* Combines the preliminary band/region precheck with the stock VHF/UHF
 * range check. The band selector is 0 through 299.99999 MHz and 1 above,
 * matching the earlier stock TX setup assignment to RAM 0x12B9D. */
bool h9_main_tx_hz_allowed_with_limits(
    const H9MainTxFrequencyPolicy *policy,
    const H9MainTxLimits *limits, uint32_t tx_hz);

#endif
