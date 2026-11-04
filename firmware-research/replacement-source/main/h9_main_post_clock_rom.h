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

/* Stock app+0x05FCB2..0x05FCD4 checks the selector-148 result. When nonzero,
 * ROM selector 192 supplies a 0..55 bitmap index; larger values leave this
 * board path. The bit clear is stock app+0x05FA44..0x05FA5C. */
typedef enum {
    H9_MAIN_POST_CLOCK_BITMAP_ERROR = 0,
    H9_MAIN_POST_CLOCK_BITMAP_CONTINUE,
    H9_MAIN_POST_CLOCK_BITMAP_ALTERNATE
} H9MainPostClockBitmapBranch;

H9MainPostClockBitmapBranch h9_main_post_clock_bitmap_step(
    H9MainPostClockRom *rom, H9MainPostClockWords *words,
    const H9MainPostClockClassification *classification);

#endif
