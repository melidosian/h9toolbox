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
void h9_spi2_setup(void);
void h9_nor_read_raw(void *buffer, uint32_t chip_address, uint32_t length);

// Stock key byte values (RAM 0xC3AF).
enum { TD_KEY_3 = 5, TD_KEY_MENU = 12, TD_KEY_UP = 13, TD_KEY_DOWN = 14, TD_KEY_BACK = 15 };

// Panel.
enum { TD_W = 160, TD_H = 128 };

// Resource flash: the upload directory and the pack (prototypes/data-upload).
#define TD_RESOURCE_ORIGIN 0xA00000u
#define TD_RESOURCE_END 0xD00000u
#define TD_DIRECTORY 0xA1F000u

static inline uint32_t td_ticks(void) { return *(volatile uint32_t *)0x1d8b0; }

// td_main.c
void td_fatal(const char *what, const char *detail) __attribute__((noreturn));
void td_fill(unsigned x0, unsigned y0, unsigned w, unsigned h, uint16_t color);
void td_text(unsigned x, unsigned y, const char *s, uint16_t fg, uint16_t bg);

// td_nor.c
void td_nor_setup(void);
int td_pack_find(uint32_t *address, uint32_t *length);
