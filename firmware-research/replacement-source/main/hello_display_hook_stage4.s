/* Own the LCD data-byte wrapper while retaining stock panel initialization
 * and lower transport. 1, 2, 3, then repeating 4 LED pulses mark progress. */
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
    r6 = 0x01e4edde
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

/* Port of the behavior at stock app+0x04EC6E. The ROM calls bracket one
 * lower-transport call; their internal role remains unverified. r4/r5/r6
 * remain live in the pixel loop, so this wrapper touches only caller-saved
 * registers and its own stack slot. The lower transport remains stock. */
hello_data_byte:
    [--sp] = rets
    sp += -4
    [sp] = r0
    r3 = 0x02001634
    call r3
    r1 = sp + 0
    r0 = 1
    r2 = 1
    r3 = 0x01e4bec4
    call r3
    r3 = 0x02001650
    call r3
    sp += 4
    pc = [sp++]

    .balign 4
hello_pixels:
    .incbin "hello_display_pixels.rgb565"
