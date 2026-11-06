/* Runs in the stock v1.0.38 my_app task immediately after LCD clear.
 * This is a display transport experiment, not the final standalone boot. */
    .section .text,"ax",@progbits
    .globl hello_display_hook
hello_display_hook:
    /* Preserve the replaced call: stock backlight/PWM initialization. */
    r0 = 0x01e79dc6
    call r0

    /* Stock LCD window writer: x0, y0, x1, y1. 66 x 8 pixels. */
    r0 = 47
    r1 = 60
    r2 = 112
    r3 = 67
    r6 = 0x01e4edde
    call r6

    /* Stock byte writer accepts one RGB565 byte in r0. */
    r4 = hello_pixels
    r5 = 1056
    r6 = 0x01e4ed6e
hello_byte_loop:
    r0 = b[r4+0] (u)
    call r6
    r4 += 1
    r5 -= 1
    if (r5 != 0) goto hello_byte_loop
hello_hold:
    goto hello_hold

    .balign 4
hello_pixels:
    .incbin "hello_display_pixels.rgb565"
