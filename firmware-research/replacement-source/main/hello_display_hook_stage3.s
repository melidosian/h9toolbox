/* Diagnostic revision: preserve r0, the stock backlight descriptor pointer.
 * 1, 2, 3, then repeating 4 LED pulses mark progress. */
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
    r6 = 0x01e4ed6e
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

    .balign 4
hello_pixels:
    .incbin "hello_display_pixels.rgb565"
