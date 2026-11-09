// SPDX-License-Identifier: GPL-2.0-or-later
// TD-H9 port of DMC100-Doom: shared declarations.
#pragma once

#include <stdint.h>

// Stock-firmware helpers wrapped in td_entry.s (cave), JieLi clang ABI.
void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1);
void h9_lcd_write(const void *buffer, unsigned length);  // big-endian RGB565, <= 128 bytes
unsigned h9_key(void);
void h9_delay(unsigned ms);
void h9_led(unsigned on);
int h9_res_read(unsigned partition, uint32_t offset, void *buffer, unsigned length);
void h9_enter_usb_loader(void);  // ROM 0x02000D68, does not return
void h9_post_core_event(unsigned code, unsigned value);  // stock app+0x2D77A: event to app_core
void h9_spi2_setup(void);
void h9_irq_off(void);  // ROM 0x02001634
void h9_irq_on(void);   // ROM 0x02001650
void h9_speaker_gate(unsigned source, unsigned on);  // stock app+0x5224C
void h9_dac_rate(void *dac, unsigned rate);  // stock app+0x433C8
void h9_dac_start(void *dac);                // stock app+0x43434
void h9_dac_write128(const void *pcm);       // stock app+0x43598: 128 bytes
void h9_play_tone(unsigned n);               // stock app+0x5231A
void h9_audio_volume(unsigned state, unsigned level);  // stock app+0x60DDA
void h9_audio_route(unsigned on);            // stock app+0x520CE
void td_sound_init(void);  // td_sound.c: speaker, DAC at 11025 Hz
void td_sound_pump(void);  // keeps the DAC about 200 ms ahead
void h9_dac_analog_on(void);                 // stock app+0x4B82E..0x4B914 (tail)
void h9_nor_read_raw(void *buffer, uint32_t chip_address, uint32_t length);

// Stock key byte values (RAM 0xC3AF). Digits read as the digit + 2 (key test,
// 2026-11-09: 1=3, 2=4, 3=5, 4=6, 5=7, 6=8).
enum { TD_KEY_1 = 3, TD_KEY_2 = 4, TD_KEY_3 = 5, TD_KEY_4 = 6, TD_KEY_5 = 7, TD_KEY_6 = 8,
       TD_KEY_MENU = 12, TD_KEY_UP = 13, TD_KEY_DOWN = 14, TD_KEY_BACK = 15 };

// Panel.
enum { TD_W = 160, TD_H = 128 };

// Resource flash: the upload directory and the pack (prototypes/data-upload).
#define TD_RESOURCE_ORIGIN 0xA00000u
#define TD_RESOURCE_END 0xD00000u
#define TD_DIRECTORY 0xA1F000u

#ifdef TD_HOST
uint32_t td_ticks(void);  // test/td_host.c
#else
static inline uint32_t td_ticks(void) { return *(volatile uint32_t *)0x1d8b0; }
#endif

// td_main.c
void td_fatal(const char *what, const char *detail) __attribute__((noreturn));
void td_fill(unsigned x0, unsigned y0, unsigned w, unsigned h, uint16_t color);
void td_text(unsigned x, unsigned y, const char *s, uint16_t fg, uint16_t bg);

// td_trace.c (diagnostic counters, drawn until the first frame)
extern volatile uint32_t td_nor_reads, td_pump_strips, td_time_calls, td_tic_calls;
void td_trace_counters(int force);

// td_nor.c
void td_nor_setup(void);
int td_pack_find(uint32_t *address, uint32_t *length);
