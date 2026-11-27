/* SPI2 resource-flash access from the BR23 mask ROM's USB Update mode.
 *
 * Uploaded to RAM 0x12000 and entered with JUMP_TO_MEMORY after U1 has shown
 * that the ROM's jump returns. The stock app is not running in this mode, so
 * the SPI2 bring-up from stock my_app is repeated here (stock v1.0.38):
 *   - SPI2 registers at 0x1E1E00: +0 control, +4 baud divider, +8 buffer
 *     (table 0x01EB2144 index 2)
 *   - port A mapping: clear bit 16 of 0x1E511C (callback 0x01E8C23A);
 *     stock pin table 0x01EB4F21 gives port A CLK 24, DO 26, DI 25
 *   - control bits from 0x01E4C1AE(2, mode 0) and 0x01E4C2DA(2):
 *     |= 8, &= ~0xC00, &= ~4, &= ~2, |= 0x10, |= 0x20, |= 0x80, &= ~0x40,
 *     |= 0x4000, baud, buffer = 0, |= 1
 *   - byte exchange from 0x01E4FF06/0x01E4BD0E: write buffer, wait for
 *     control bit 15, set bit 14 to clear it, read buffer
 *   - chip select GPIO 23 (bank 1 bit 7 at 0x1E5040) as in stock
 *     app+0x0884A8: input enable, output, high; each pin write is bracketed
 *     by a read of 0x1E0200 like stock
 * The ROM lock pair stock uses (0x02001634/0x02001650) belongs to the running
 * app's scheduler and is not called here.
 *
 * U2 on hardware (2026-11-08): op 1 completed every byte (SPI2 is clocked in
 * update mode) but read JEDEC FFFFFF, so the pins are not set up there. Ops
 * 3-5 diagnose that. GPIO bank registers: +0 out, +4 in (stock app+0x052558
 * reads [bank+4]), +8 direction (1 = input), +12 input enable, +16 pull-up,
 * +20 pull-down.
 *
 * The host fills the parameter block at RAM 0x13040 before each jump:
 *   +0  magic "H9U2"
 *   +4  op: 1 JEDEC ID, 2 read, 3 register snapshot, 4 JEDEC ID after
 *       explicit pin setup, 5 bit-banged JEDEC ID
 *   +32 bit-bang MOSI mask, +36 MISO mask (op 5: 0x400/0x200 or 0x200/0x400)
 *   op 7 blinks GPIO 9 (the top LED, proven by the boot probe) twice so a
 *       person can see whether GPIO outputs drive pads in USB Update mode
 *   +40 op 6 only: the bank-1 input word is stored at RAM 0x13600 before CS
 *       falls and after every clock rise (1 + 32 words); op 5 stores nothing
 *   +8  chip address (24-bit)   +12 length (1-4096, read only)
 *   +16 baud divider (0-255)
 * and reads back:
 *   +20 status: 0 ok, 1 bad magic, 2 SPI timeout, 3 bad op or length
 *   +24 JEDEC ID (manufacturer << 16 | type << 8 | capacity)
 *   +28 completed operations
 * Read data lands at RAM 0x13400..0x143FF. Op 3 stores GPIO banks 0-3
 * (0x1E5000..0x1E50FF, 64 words) at 0x13400, 0x1E5100..0x1E511C (8 words) at
 * 0x13500, and control/baud of SPI0 (0x1E1C00), SPI1 (0x1E1D00) and SPI2
 * (0x1E1E00) at 0x13520/0x13528/0x13530. It only reads registers. Only
 * read-type SPI NOR commands (9F, 03) are ever sent.
 */
    .section .text,"ax",@progbits
    .globl entry
entry:
    [--sp] = {rets, r7-r4}
    r4 = 0x00013040
    r5 = 1
    r0 = [r4+0]
    r1 = 0x32553948
    if (r0 != r1) goto finish
    r5 = 3
    r0 = [r4+4]
    if (r0 == 3) goto op_regs
    if (r0 == 4) goto op_pins_jedec
    if (r0 == 5) goto op_bitbang
    if (r0 == 6) goto op_bitbang
    if (r0 == 7) goto op_led
    call spi2_open
    r0 = [r4+4]
    if (r0 == 1) goto op_jedec
    if (r0 == 2) goto op_read
    goto finish

op_pins_jedec:
    call pins_init
    call spi2_open
op_jedec:
    call cs_low
    r0 = 0x9f
    call xfer
    if (r1 != 0) goto timeout
    r0 = 0xff
    call xfer
    if (r1 != 0) goto timeout
    r6 = r0
    r0 = 0xff
    call xfer
    if (r1 != 0) goto timeout
    r6 = r6 << 8
    r6 |= r0
    r0 = 0xff
    call xfer
    if (r1 != 0) goto timeout
    r6 = r6 << 8
    r6 |= r0
    call cs_high
    [r4+24] = r6
    r5 = 0
    goto finish

op_read:
    r6 = [r4+12]
    if (r6 == 0) goto finish
    r0 = 4096
    if (r6 > r0) goto finish
    r7 = [r4+8]
    call cs_low
    r0 = 0x03
    call xfer
    if (r1 != 0) goto timeout
    r0 = r7 >> 16
    r0 = r0 & 255
    call xfer
    if (r1 != 0) goto timeout
    r0 = r7 >> 8
    r0 = r0 & 255
    call xfer
    if (r1 != 0) goto timeout
    r0 = r7 & 255
    call xfer
    if (r1 != 0) goto timeout
    r7 = 0x00013400
read_byte:
    r0 = 0xff
    call xfer
    if (r1 != 0) goto timeout
    b[r7+0] = r0
    r7 += 1
    r6 += -1
    if (r6 != 0) goto read_byte
    call cs_high
    r5 = 0
    goto finish

op_regs:
    r0 = 0x001e5000
    r1 = 0x00013400
    r2 = 64
    call copy_words
    r0 = 0x001e5100
    r1 = 0x00013500
    r2 = 8
    call copy_words
    r0 = 0x001e1c00
    r1 = 0x00013520
    r2 = 2
    call copy_words
    r0 = 0x001e1d00
    r1 = 0x00013528
    r2 = 2
    call copy_words
    r0 = 0x001e1e00
    r1 = 0x00013530
    r2 = 2
    call copy_words
    r5 = 0
    goto finish

/* Bit-banged 9F on CS 23 and CLK 24, SPI2 disabled. The data pins come
 * from the parameter block (+32 MOSI mask, +36 MISO mask) and must be bank-1
 * bits 9 and 10 (GPIO 25/26) in either order. Both start as inputs; MOSI is
 * driven only during the command byte, so neither order can make the MCU
 * fight the flash's output. */
op_bitbang:
    r0 = [r4+32]
    r1 = [r4+36]
    r2 = r0 | r1
    r3 = 0x600
    if (r2 != r3) goto finish
    if (r0 == r1) goto finish
    r3 = 0x001e1e00
    [r3+0] &= 0xfffffffe
    call bb_pins_init
    call cs_init
    r3 = 0x001e5040
    r0 = [r3+4]
    r1 = 0x00013600
    [r1+0] = r0
    r1 += 4
    [r4+40] = r1
    call cs_low
    r3 = 0x001e5040
    r1 = [r4+32]
    r2 = ~r1
    r0 = [r3+8]
    r0 &= r2
    [r3+8] = r0
    r0 = 0x9f
    r1 = 1
    call bb_byte
    r3 = 0x001e5040
    r1 = [r4+32]
    r0 = [r3+8]
    r0 |= r1
    [r3+8] = r0
    r6 = 0
    r7 = 3
bb_id_byte:
    r0 = 0
    r1 = 0
    call bb_byte
    r6 = r6 << 8
    r6 |= r0
    r7 += -1
    if (r7 != 0) goto bb_id_byte
    call cs_high
    [r4+24] = r6
    r5 = 0
    goto finish

/* GPIO 9 = bank 0 bit 9: input enable, output, two on/off pulses, then off. */
op_led:
    r3 = 0x001e5000
    [r3+12] |= 0x200
    [r3+8] &= 0xfffffdff
    r6 = 2
led_pulse:
    [r3+0] |= 0x200
    r0 = 4000000
    call long_delay
    [r3+0] &= 0xfffffdff
    r0 = 4000000
    call long_delay
    r6 += -1
    if (r6 != 0) goto led_pulse
    r5 = 0
    goto finish

long_delay:
    r0 += -1
    if (r0 != 0) goto long_delay
    rts

timeout:
    call cs_high
    r5 = 2
finish:
    [r4+20] = r5
    r0 = [r4+28]
    r0 += 1
    [r4+28] = r0
    {pc, r7-r4} = [sp++]

/* SPI2 on port A, configured as stock does for the resource flash. */
spi2_open:
    [--sp] = rets
    call cs_init
    r3 = 0x001e511c
    [r3+0] &= 0xfffeffff
    r3 = 0x001e1e00
    [r3+0] |= 0x8
    r0 = [r3+0]
    r1 = 0xfffff3ff
    r0 = r0 & r1
    [r3+0] = r0
    [r3+0] &= 0xfffffffb
    [r3+0] &= 0xfffffffd
    [r3+0] |= 0x10
    [r3+0] |= 0x20
    [r3+0] |= 0x80
    [r3+0] &= 0xffffffbf
    [r3+0] |= 0x4000
    r0 = 0x00013040
    r0 = [r0+16]
    r0 = r0 & 255
    [r3+4] = r0
    r0 = 0
    [r3+8] = r0
    [r3+0] |= 0x1
    pc = [sp++]

/* Chip select GPIO 23 as stock app+0x0884A8: input enable, output, high. */
cs_init:
    r2 = 0x001e0200
    r3 = 0x001e5040
    r0 = [r2+0]
    [r3+12] |= 0x80
    r0 = [r2+0]
    [r3+8] &= 0xffffff7f
    r0 = [r2+0]
    [r3+0] |= 0x80
    r0 = [r2+0]
    rts

/* SDK-style SPI pin setup (input enable on, pulls off): CLK 24 output idle
 * low, DO 26 output, DI 25 input. Bank 1 bits 8, 10, 9. */
pins_init:
    r2 = 0x001e0200
    r3 = 0x001e5040
    r0 = [r2+0]
    [r3+0] &= 0xfffffeff
    [r3+8] &= 0xfffffeff
    [r3+8] &= 0xfffffbff
    [r3+8] |= 0x200
    [r3+12] |= 0x100
    [r3+12] |= 0x200
    [r3+12] |= 0x400
    [r3+16] &= 0xfffffeff
    [r3+16] &= 0xfffffdff
    [r3+16] &= 0xfffffbff
    [r3+20] &= 0xfffffeff
    [r3+20] &= 0xfffffdff
    [r3+20] &= 0xfffffbff
    r0 = [r2+0]
    rts

/* Copy r2 words from r0 to r1. */
copy_words:
    r3 = [r0+0]
    [r1+0] = r3
    r0 += 4
    r1 += 4
    r2 += -1
    if (r2 != 0) goto copy_words
    rts

/* CLK 24 output idle low; GPIO 25 and 26 inputs; input enable on, pulls off. */
bb_pins_init:
    r2 = 0x001e0200
    r3 = 0x001e5040
    r0 = [r2+0]
    [r3+0] &= 0xfffffeff
    [r3+8] &= 0xfffffeff
    [r3+8] |= 0x200
    [r3+8] |= 0x400
    [r3+12] |= 0x100
    [r3+12] |= 0x200
    [r3+12] |= 0x400
    [r3+16] &= 0xfffffeff
    [r3+16] &= 0xfffffdff
    [r3+16] &= 0xfffffbff
    [r3+20] &= 0xfffffeff
    [r3+20] &= 0xfffffdff
    [r3+20] &= 0xfffffbff
    r0 = [r2+0]
    rts

/* Mode-0 bit-bang byte: r0 = byte out, r1 = drive MOSI; returns byte in.
 * MOSI changes while CLK is low; MISO is sampled after CLK rises. Masks come
 * from the parameter block (+32 MOSI, +36 MISO). */
bb_byte:
    [--sp] = {rets, r7-r4}
    r4 = r0
    r5 = r1
    r6 = 0
    r7 = 8
    r3 = 0x001e5040
bb_bit:
    if (r5 == 0) goto bb_clock
    r1 = 0x00013040
    r1 = [r1+32]
    r0 = [r3+0]
    r2 = r4 >> 7
    r2 = r2 & 1
    if (r2 == 0) goto bb_mosi_low
    r0 |= r1
    goto bb_mosi_store
bb_mosi_low:
    r1 = ~r1
    r0 &= r1
bb_mosi_store:
    [r3+0] = r0
bb_clock:
    call bb_delay
    [r3+0] |= 0x100
    call bb_delay
    r0 = [r3+4]
    r1 = 0x00013040
    r2 = [r1+4]
    if (r2 != 6) goto bb_sampled
    r2 = [r1+40]
    [r2+0] = r0
    r2 += 4
    [r1+40] = r2
bb_sampled:
    r1 = [r1+36]
    r0 &= r1
    r6 = r6 << 1
    if (r0 == 0) goto bb_clock_low
    r0 = 1
    r6 |= r0
bb_clock_low:
    [r3+0] &= 0xfffffeff
    r4 = r4 << 1
    r7 += -1
    if (r7 != 0) goto bb_bit
    r0 = r6 & 255
    {pc, r7-r4} = [sp++]

bb_delay:
    r0 = 64
bb_delay_loop:
    r0 += -1
    if (r0 != 0) goto bb_delay_loop
    rts

cs_low:
    r2 = 0x001e0200
    r3 = 0x001e5040
    r0 = [r2+0]
    [r3+0] &= 0xffffff7f
    r0 = [r2+0]
    rts

cs_high:
    r2 = 0x001e0200
    r3 = 0x001e5040
    r0 = [r2+0]
    [r3+0] |= 0x80
    r0 = [r2+0]
    rts

/* Exchange byte r0; returns the received byte in r0, and r1 = 0 on success
 * or 1 if control bit 15 never set within the bounded wait. */
xfer:
    r3 = 0x001e1e00
    [r3+8] = r0
    r2 = 200000
xfer_wait:
    r0 = h[r3+0] (u)
    r1 = r0 >> 15
    if (r1 != 0) goto xfer_done
    r2 += -1
    if (r2 != 0) goto xfer_wait
    r1 = 1
    rts
xfer_done:
    [r3+0] |= 0x4000
    r0 = [r3+8]
    r0 = r0 & 255
    r1 = 0
    rts
