#include "h9_main_tx_frequency.h"

#include <stddef.h>

const H9MainTxLimits h9_main_stock_tx_limits = {
    13600000u, 17400000u, 40000000u, 52000000u
};

/* Exact 30-word mode-2 table at app+0x0B4F78, in 10 Hz units. */
static const uint32_t mode2_frequency_10hz[30] = {
    46256250u, 46258750u, 46261250u, 46263750u,
    46266250u, 46268750u, 46271250u, 46756250u,
    46758750u, 46761250u, 46763750u, 46766250u,
    46768750u, 46771250u, 46255000u, 46257500u,
    46260000u, 46262500u, 46265000u, 46267500u,
    46270000u, 46272500u, 46755000u, 46757500u,
    46760000u, 46762500u, 46765000u, 46767500u,
    46770000u, 46772500u
};

bool h9_main_tx_apply_shift(uint32_t base_10hz, bool simplex,
                             uint8_t shift_mode, uint32_t offset_10hz,
                             uint32_t *tx_10hz)
{
    if (tx_10hz == NULL || base_10hz == 0u || shift_mode > 2u)
        return false;
    if (simplex || shift_mode == 0u) {
        *tx_10hz = base_10hz;
        return true;
    }
    if (shift_mode == 1u) {
        if (offset_10hz >= base_10hz) return false;
        *tx_10hz = base_10hz - offset_10hz;
        return true;
    }
    if (offset_10hz > UINT32_MAX - base_10hz) return false;
    *tx_10hz = base_10hz + offset_10hz;
    return true;
}

bool h9_main_tx_frequency_allowed(const H9MainTxFrequencyPolicy *policy,
                                   uint32_t tx_10hz)
{
    size_t index;
    if (policy == NULL || tx_10hz < 13600000u) return false;
    if (tx_10hz >= 20000000u && tx_10hz < 30000000u &&
        !policy->allow_200_to_300_mhz) return false;
    if (tx_10hz >= 30000000u && tx_10hz < 40000000u &&
        !policy->allow_300_to_400_mhz) return false;
    if (tx_10hz >= 50000000u && !policy->allow_500_mhz_plus)
        return false;
    if (policy->region_mode == 0u) return true;
    if (policy->region_mode == 1u)
        return (tx_10hz >= 14400000u && tx_10hz <= 14800000u) ||
               (tx_10hz >= 42000000u && tx_10hz <= 45000000u);
    if (policy->region_mode != 2u) return false;
    for (index = 0u; index < 30u; ++index)
        if (tx_10hz == mode2_frequency_10hz[index]) return true;
    return false;
}

bool h9_main_tx_hz_allowed(const H9MainTxFrequencyPolicy *policy,
                            uint32_t tx_hz)
{
    return tx_hz != 0u && tx_hz % 10u == 0u &&
           h9_main_tx_frequency_allowed(policy, tx_hz / 10u);
}

bool h9_main_tx_limit_check(uint32_t *frequency_10hz,
                             const H9MainTxLimits *limits,
                             uint8_t range_selector,
                             bool normalize_to_lower,
                             bool allow_200_to_300_mhz)
{
    uint32_t low;
    uint32_t high;
    uint32_t base;
    uint32_t candidate;
    if (frequency_10hz == NULL || limits == NULL || range_selector > 1u)
        return false;
    low = range_selector == 0u ? limits->range0_low_10hz :
                                  limits->range1_low_10hz;
    high = range_selector == 0u ? limits->range0_high_10hz :
                                   limits->range1_high_10hz;
    if (low == 0u || high < low) return false;
    base = normalize_to_lower ? low : 0u;
    low -= base;
    high -= base;
    candidate = *frequency_10hz;
    if (candidate > high &&
        !(range_selector == 0u && allow_200_to_300_mhz &&
          candidate >= 20000000u && candidate < 30000000u)) {
        *frequency_10hz = low;
        return false;
    }
    if (candidate < low) {
        *frequency_10hz = high;
        return false;
    }
    return true;
}

bool h9_main_tx_hz_allowed_with_limits(
    const H9MainTxFrequencyPolicy *policy,
    const H9MainTxLimits *limits, uint32_t tx_hz)
{
    uint32_t candidate;
    if (!h9_main_tx_hz_allowed(policy, tx_hz) || limits == NULL)
        return false;
    candidate = tx_hz / 10u;
    return h9_main_tx_limit_check(&candidate, limits,
                                  candidate > 29999999u ? 1u : 0u, false,
                                  policy->allow_200_to_300_mhz);
}
