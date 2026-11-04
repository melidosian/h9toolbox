#ifndef H9_MAIN_APRS_CONTROL_H
#define H9_MAIN_APRS_CONTROL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "h9_main_aprs_receive.h"

/* Raw group/opcode pairs routed by stock main app+0x0870BC. */
typedef enum {
    H9_MAIN_CONTROL_NONE = 0,
    H9_MAIN_CONTROL_06_01 = 0x0601,
    H9_MAIN_CONTROL_06_02 = 0x0602,
    H9_MAIN_CONTROL_06_03 = 0x0603,
    H9_MAIN_CONTROL_06_04 = 0x0604,
    H9_MAIN_CONTROL_06_05 = 0x0605,
    H9_MAIN_CONTROL_06_06 = 0x0606,
    H9_MAIN_CONTROL_07_01 = 0x0701,
    H9_MAIN_CONTROL_07_03 = 0x0703
} H9MainControlAction;

/* Stock CRC: initial 0xFFFF, polynomial 0x1021, no reflection or final XOR. */
uint16_t h9_main_control_crc(const uint8_t *data, size_t length);

/* Returns 1 only for a complete command-0x51 frame with valid CRC and a
 * recognized group/opcode. No stock side effect is performed here. */
int h9_main_control_from_frame(const H9MainWireReceiver *receiver,
                               H9MainControlAction *action);

/* The 06xx commands feed stock app+0x078E94 and app+0x087086. The caller
 * supplies the current 0..30 level from its settings store. These effects
 * describe work for the board/UI adapter; no audio or timer hardware is
 * touched by this reducer. */
typedef struct {
    uint8_t level;
    /* Stock keeps the saved setting at 0xC0F9 separately from RAM 0x11B16. */
    uint8_t saved_level;
    uint8_t held_adjustment;
    bool timer_armed;
    bool volume_display_pending;
    /* Mirrors the gate at RAM 0x11AE8; its physical meaning is unmapped. */
    bool defer_output_updates;
    bool fm_inside_enabled;
} H9MainControlVolumeState;

typedef struct {
    bool level_changed;
    bool setting_written;
    bool reconfigure_5050a;
    bool apply_audio_60eda;
    bool notify_level_71b58;
    bool refresh_ui_70758;
    bool bracket_fm_inside_65fa6;
    bool boundary_feedback;
    bool cancel_timer;
    uint16_t start_timer_units;
} H9MainControlVolumeEffects;

bool h9_main_control_volume_init(H9MainControlVolumeState *state,
                                  uint8_t initial_level);
bool h9_main_control_volume_apply(H9MainControlVolumeState *state,
                                   H9MainControlAction action,
                                   H9MainControlVolumeEffects *effects);

/* Stock 07 03 changes these RAM fields at 0x12674, 0x12C53/4, and 0x12C60.
 * The timer callback's branch targets are retained as address-named effects
 * until their application semantics are mapped. */
typedef struct {
    uint8_t flags;
    uint8_t pending;
    uint8_t phase;
    uint32_t timer_handle;
} H9MainControl07State;

typedef struct {
    bool start_timer;
    uint16_t delay_units;
} H9MainControl07StartEffects;

typedef enum {
    H9_MAIN_CONTROL_07_CALLBACK_NONE,
    H9_MAIN_CONTROL_07_CALL_6172C_WITH_2,
    H9_MAIN_CONTROL_07_CALL_ROM_0200010A
} H9MainControl07Callback;

typedef struct {
    uint16_t timer_handle_low16;
    H9MainControl07Callback callback;
} H9MainControl07TimerEffects;

bool h9_main_control_07_03(H9MainControl07State *state,
                           H9MainControl07StartEffects *effects);
bool h9_main_control_07_timer_fired(H9MainControl07State *state,
                                    uint8_t callback_parameter,
                                    H9MainControl07TimerEffects *effects);

#endif
