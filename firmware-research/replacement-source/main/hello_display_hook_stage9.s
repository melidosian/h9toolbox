/* Stage nine: stock panel initialization, authored brightness calculation.
 * Own the LCD window, command, word, byte, and transfer-submit wrappers.
 * LED milestones: 1, 2, 3, then repeating 4 pulses. */
    .section .text,"ax",@progbits
    .globl hello_display_hook
hello_display_hook:
    r8 = r0
    /* Stock breathing LED GPIO9 setup, previously boot-verified. */
    r7 = 0x001e5000
    r1 = [r7+0]
    r1 = r1 & ~0x200
    [r7+0] = r1
    r1 = [r7+12]
    r1 = r1 | 0x200
    [r7+12] = r1
    r1 = [r7+8]
    r1 = r1 & ~0x200
    [r7+8] = r1

    r0 = 1
    call led_pattern
    /* Stock my_app built this descriptor immediately before the replaced
     * call. Rewrite each observed field with authored values. This retains
     * stock panel initialization and computes duty from the saved setting. */
    r0 = r8
    r1 = 1000
    [r0+0] = r1
    r1 = 2
    [r0+4] = r1
    [r0+8] = r1
    /* Stock my_app reads 0x12BAA and computes 2000 + 2000 * setting. */
    r1 = 0x12baa
    r1 = b[r1+0] (u)
    r1 = r1 * 0x7d0
    r1 = r1 + 2000
    h[r0+12] = r1
    r1 = 0
    [r0+16] = r1
    r1 = 1872
    h[r0+20] = r1
    r1 = 1
    b[r0+24] = r1

    /* The replaced stock call consumes r0 as a descriptor pointer. */
    r0 = r8
    r6 = 0x01e79dc6
    call r6
    r0 = 2
    call led_pattern

    r0 = 47
    r1 = 60
    r2 = 112
    r3 = 67
    r6 = hello_set_window
    call r6
    r0 = 3
    call led_pattern

    r4 = hello_pixels
    r5 = 1056
    r6 = hello_data_byte
hello_byte_loop:
    r0 = b[r4+0] (u)
    call r6
    r4 += 1
    r5 -= 1
    if (r5 != 0) goto hello_byte_loop
hello_complete:
    r0 = 4
    call led_pattern
    goto hello_complete

led_pattern:
    [--sp] = {rets, r5, r4}
    r4 = r0
led_pulse:
    r1 = [r7+0]
    r1 = r1 | 0x200
    [r7+0] = r1
    r5 = 1500000
led_on_delay:
    r5 -= 1
    if (r5 != 0) goto led_on_delay
    r1 = [r7+0]
    r1 = r1 & ~0x200
    [r7+0] = r1
    r5 = 1500000
led_off_delay:
    r5 -= 1
    if (r5 != 0) goto led_off_delay
    r4 -= 1
    if (r4 != 0) goto led_pulse
    r5 = 4000000
led_gap:
    r5 -= 1
    if (r5 != 0) goto led_gap
    {pc, r5, r4} = [sp++]

/* 0x2A column range, 0x2B row range, 0x2C pixel-memory write.
 * The stock window routine at app+0x04ECDE uses this same coordinate order. */
hello_set_window:
    [--sp] = {rets, r7-r4}
    r4 = r3
    r5 = r2
    r6 = r1
    r7 = r0
    r0 = 42
    call hello_command
    r0 = r7
    call hello_data_word
    r0 = r5
    call hello_data_word
    r0 = 43
    call hello_command
    r0 = r6
    call hello_data_word
    r0 = r4
    call hello_data_word
    r0 = 44
    call hello_command
    {pc, r7-r4} = [sp++]

/* Stock app+0x04EC8A selects pin 18 and clears bit 4 at its GPIO bank
 * while sending a command byte. The sync register reads bracket pin changes.
 * Keep the command byte in r2 through the reads. */
hello_command:
    [--sp] = {rets, r5, r4}
    r2 = r0
    r0 = 18
    r3 = 0x01e001ba
    call r3
    r4 = r0
    if (r4 == 0) goto hello_command_done
    r5 = 0x001e0200
    r3 = [r5+0]
    r3 = [r4+0]
    r3 = r3 & ~0x4
    [r4+0] = r3
    r3 = [r5+0]
    r0 = r2
    call hello_data_byte
    r3 = [r5+0]
    r3 = [r4+0]
    r3 = r3 | 0x4
    [r4+0] = r3
    r3 = [r5+0]
hello_command_done:
    {pc, r5, r4} = [sp++]

/* Stock app+0x04ECBA sends one big-endian coordinate word through the
 * same transfer-submit path used by the stage-four byte wrapper. */
hello_data_word:
    [--sp] = rets
    sp += -4
    r1 = r0 >> 8
    b[sp] = r1
    b[sp+1] = r0
    r3 = 0x02001634
    call r3
    r1 = sp + 0
    r0 = 1
    r2 = 2
    r3 = hello_transfer_submit
    call r3
    r3 = 0x02001650
    call r3
    sp += 4
    pc = [sp++]

/* Port of the behavior at stock app+0x04EC6E. The ROM calls bracket one
 * transfer-submit call; their internal role remains unverified. r4/r5/r6
 * remain live in the pixel loop, so this wrapper touches only caller-saved
 * registers and its own stack slot. */
hello_data_byte:
    [--sp] = rets
    sp += -4
    [sp] = r0
    r3 = 0x02001634
    call r3
    r1 = sp + 0
    r0 = 1
    r2 = 1
    r3 = hello_transfer_submit
    call r3
    r3 = 0x02001650
    call r3
    sp += 4
    pc = [sp++]

/* Port of stock app+0x04BDC4: normalize controller index, select its
 * register block, submit buffer pointer and length, then invoke the stock
 * completion path at 0x01E4BD0E. This does not initialize the controller. */
hello_transfer_submit:
    [--sp] = {rets, r6-r4}
    r4 = r2
    r5 = r1
    r6 = r0
    r3 = 0x01e4bc8e
    call r3
    r1 = 0x01eb2144
    r0 = [r1+r0<<2]
    [r0+0] &= 0xffffefff
    [r0+12] = r5
    [r0+16] = r4
    csync
    r0 = r6
    r1 = r4
    r3 = 0x01e4bd0e
    call r3
    {pc, r6-r4} = [sp++]

    .balign 4
hello_pixels:
    .incbin "hello_display_pixels.rgb565"
