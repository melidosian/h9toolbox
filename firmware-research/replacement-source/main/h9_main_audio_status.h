#ifndef H9_MAIN_AUDIO_STATUS_H
#define H9_MAIN_AUDIO_STATUS_H

#include <stdbool.h>
#include <stdint.h>

/* Stock app+0x01677C scans two 28-byte records at RAM 0x1D3DC and
 * 0x1D3F8. The record fields are kept opaque until their owner is mapped. */
#define H9_MAIN_AUDIO_RECORD_SIZE 28u

typedef struct {
    uint8_t bytes[H9_MAIN_AUDIO_RECORD_SIZE];
} H9MainAudioRecord;

/* Stock app+0x016728 calls an external six-byte comparator against the
 * key at RAM 0x1D43C when both records qualify. Return true only when
 * that comparison succeeds. */
typedef bool (*H9MainAudioKeyMatch)(void *context,
                                    const H9MainAudioRecord *record);

typedef struct {
    H9MainAudioRecord records[2];
    H9MainAudioKeyMatch key_match;
    void *key_context;
} H9MainAudioStatusSnapshot;

/* selector 0 chooses an ordinary active record; selector 1 chooses the
 * first record with either 0x2080 flag. Other selectors use the stock
 * comparator fallback. A missing comparator cannot resolve ambiguity. */
const H9MainAudioRecord *h9_main_audio_select_record(
    const H9MainAudioStatusSnapshot *snapshot, unsigned selector);

/* Stock app+0x01682C: byte 14 of selector 0, or 0x2B if absent. */
uint8_t h9_main_audio_status(const H9MainAudioStatusSnapshot *snapshot);

#endif
