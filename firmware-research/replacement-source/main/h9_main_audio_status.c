#include "h9_main_audio_status.h"

#include <stddef.h>

static uint16_t word_at(const H9MainAudioRecord *record, unsigned offset)
{
    return (uint16_t)((uint16_t)record->bytes[offset] |
                      ((uint16_t)record->bytes[offset + 1u] << 8));
}

static bool active(const H9MainAudioRecord *record)
{
    return (record->bytes[0x12] & 0x20u) != 0u &&
           word_at(record, 6u) != 0u;
}

static const H9MainAudioRecord *fallback(
    const H9MainAudioStatusSnapshot *snapshot)
{
    unsigned i;
    if (snapshot->key_match == NULL) return NULL;
    for (i = 0u; i < 2u; ++i) {
        const H9MainAudioRecord *record = &snapshot->records[i];
        /* The stock helper tests bit 29 of the same 32-bit field whose
         * low byte is record+0xF. */
        if (snapshot->key_match(snapshot->key_context, record) &&
            (record->bytes[0x12] & 0x20u) != 0u) return record;
    }
    return NULL;
}

const H9MainAudioRecord *h9_main_audio_select_record(
    const H9MainAudioStatusSnapshot *snapshot, unsigned selector)
{
    const H9MainAudioRecord *selected = NULL;
    unsigned count = 0u;
    unsigned i;
    if (snapshot == NULL) return NULL;
    if (selector == 1u) {
        for (i = 0u; i < 2u; ++i) {
            const H9MainAudioRecord *record = &snapshot->records[i];
            if (active(record) &&
                (word_at(record, 8u) & 0x2080u) != 0u) return record;
        }
        return NULL;
    }
    if (selector == 0u) {
        for (i = 0u; i < 2u; ++i) {
            const H9MainAudioRecord *record = &snapshot->records[i];
            if (active(record) &&
                (word_at(record, 8u) & 0x2080u) == 0u) {
                selected = record;
                ++count;
            }
        }
        if (count == 0u) return NULL;
        if (count == 1u) return selected;
    }
    return fallback(snapshot);
}

uint8_t h9_main_audio_status(const H9MainAudioStatusSnapshot *snapshot)
{
    const H9MainAudioRecord *record = h9_main_audio_select_record(snapshot, 0u);
    return record == NULL ? 0x2Bu : record->bytes[0xEu];
}
