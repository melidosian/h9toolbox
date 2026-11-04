#include "h9_main_rf_registers.h"

#include <stddef.h>
#include <stdint.h>

bool h9_main_rf_frequency_words(uint32_t frequency_10hz,
                                uint8_t band_selector,
                                int16_t lower_trim,
                                int16_t upper_trim,
                                uint16_t *low_word,
                                uint16_t *high_word)
{
    int32_t correction = 0;
    int64_t adjusted;
    if (low_word == NULL || high_word == NULL) return false;
    if (band_selector == 0u) correction = lower_trim / 10;
    else if (band_selector == 1u) correction = upper_trim / 10;
    adjusted = (int64_t)frequency_10hz + correction;
    if (adjusted <= 0 || adjusted > UINT32_MAX) return false;
    *low_word = (uint16_t)adjusted;
    *high_word = (uint16_t)((uint64_t)adjusted >> 16);
    return true;
}

bool h9_main_rf_mode_word(uint8_t path, uint32_t mode,
                          uint8_t *register_number, uint16_t *word)
{
    uint16_t value = 0xF50Bu;
    if (register_number == NULL || word == NULL || path > 1u)
        return false;
    switch (mode & 0xFFFFFF0Fu) {
    case 1u: value = 0xF61Cu; break;
    case 2u: value = 0xDF22u; break;
    case 3u: value = 0xD42Du; break;
    case 4u: value = 0xCC35u; break;
    case 13u: value = 0xFEF0u; break;
    case 14u: value = 0xFCFAu; break;
    case 15u: value = 0xFA02u; break;
    default: break;
    }
    *register_number = path == 0u ? 0x75u : 0x74u;
    *word = value;
    return true;
}

bool h9_main_rf_write_frequency(uint32_t frequency_10hz,
                                 uint8_t band_selector,
                                 int16_t lower_trim,
                                 int16_t upper_trim,
                                 H9MainRfWrite write_register,
                                 void *opaque)
{
    uint16_t low_word;
    uint16_t high_word;
    if (write_register == NULL ||
        !h9_main_rf_frequency_words(frequency_10hz, band_selector,
                                    lower_trim, upper_trim,
                                    &low_word, &high_word)) return false;
    if (!write_register(opaque, 0x38u, low_word)) return false;
    return write_register(opaque, 0x39u, high_word);
}
