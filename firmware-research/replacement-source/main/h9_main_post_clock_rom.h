#ifndef H9_MAIN_POST_CLOCK_ROM_H
#define H9_MAIN_POST_CLOCK_ROM_H

#include <stdbool.h>
#include <stdint.h>

/* Board entry app+0x05FC24..0x05FC4C, immediately after the clock helper. */
typedef struct {
    void *context;
    uint32_t (*read_selector)(void *context, uint32_t selector);
    void (*set_control)(void *context, uint32_t group, uint32_t value);
} H9MainPostClockRom;

typedef struct {
    uint32_t first_word;
    uint32_t second_word;
} H9MainPostClockWords;

void h9_main_post_clock_rom_bind_target(H9MainPostClockRom *rom);
bool h9_main_post_clock_rom_run(H9MainPostClockRom *rom,
                                H9MainPostClockWords *words);

/* Stock board entry app+0x05FC4C..0x05FCB2. Strap indices outside the two
 * primary masks take a fallback that sets mode 1 and rejoins setup. */
typedef struct {
    volatile uint32_t *boot_flags_100100;
    volatile uint32_t *channel_status_1e511c;
    volatile uint32_t *strap_status_1f0200;
    volatile uint32_t *secondary_status_1f0500;
    volatile uint8_t *mode_11b43;
    H9MainPostClockRom *rom;
} H9MainPostClockClassifier;

typedef struct {
    uint32_t channel_flag;
    uint32_t selector_148_masked;
    uint32_t secondary_status;
    uint8_t strap_index;
    uint8_t mode;
} H9MainPostClockClassification;

void h9_main_post_clock_classifier_bind_target(
    H9MainPostClockClassifier *classifier, H9MainPostClockRom *rom);
bool h9_main_post_clock_classify(H9MainPostClockClassifier *classifier,
                                 H9MainPostClockClassification *result);

/* Stock app+0x05FCB2..0x05FCD4 checks selector 148. Selector 192 values
 * 0..55 clear a bitmap bit; 56 and 57 set different later-board flags;
 * other values skip the bit clear. All paths rejoin at app+0x05FCD4.
 * These are the stock r6/r9 flags, not yet assigned hardware meanings. */
typedef struct {
    bool r6_flag;
    bool r9_flag;
} H9MainPostClockRouteFlags;

typedef enum {
    H9_MAIN_POST_CLOCK_BITMAP_ERROR = 0,
    H9_MAIN_POST_CLOCK_BITMAP_CONTINUE,
    H9_MAIN_POST_CLOCK_BITMAP_NO_CLEAR
} H9MainPostClockBitmapBranch;

H9MainPostClockBitmapBranch h9_main_post_clock_bitmap_step(
    H9MainPostClockRom *rom, H9MainPostClockWords *words,
    const H9MainPostClockClassification *classification,
    H9MainPostClockRouteFlags *flags);

/* Stock app+0x05FCD4..0x05FCEA passes a mask through ROM 0x02000A78,
 * duplicates its result in r0/r1, and calls ROM 0x02000D18. The second
 * call returns two words in r0/r1; their board meaning is still unknown. */
typedef struct {
    void *context;
    uint32_t (*transform_mask)(void *context, uint32_t mask);
    uint64_t (*query_pair)(void *context, uint32_t r0, uint32_t r1);
} H9MainPostClockHandoff;

typedef struct {
    uint32_t input_mask;
    uint32_t transformed_mask;
    uint32_t result_r0;
    uint32_t result_r1;
} H9MainPostClockHandoffResult;

void h9_main_post_clock_handoff_bind_target(H9MainPostClockHandoff *handoff);
bool h9_main_post_clock_handoff_run(
    H9MainPostClockHandoff *handoff, uint32_t secondary_status,
    H9MainPostClockHandoffResult *result);

#endif
