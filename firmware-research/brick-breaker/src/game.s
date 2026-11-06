/* Brick Breaker game body for the stock-UI Back-hold launcher.
 * Gameplay bytes match the hardware-tested stage-26 body exactly.
 * Stock reset/app_core, controller setup, and lower LCD transfer remain. */
    .section .text,"ax",@progbits
    .globl hello_display_hook
hello_display_hook:
    /* GPIO9 breathing LED was proven from the replacement reset probe. */
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

    /* Stock my_app waits 500 units before its pin and controller work. */
    r0 = 500
    r3 = 0x01e516c6
    call r3

    /* Preserve the five stock early pin registrations, since their board
     * functions have not all been identified. */
    r6 = 0x01e79ce4
    r0 = 12
    r1 = 4
    call r6
    r0 = 37
    r1 = 13
    call r6
    r0 = 36
    r1 = 10
    call r6
    r0 = 10
    r1 = 3
    call r6
    r0 = 1
    r1 = 0
    call r6

    /* Stock app+0x0881B2 invokes the controller setup with bus index 1.
     * This bridge retains stock app_core setup until its prerequisites are
     * understood; it never enters stock my_app. */
    r0 = 1
    r3 = 0x01e4c2da
    call r3
    .globl brick_breaker_after_stock_setup
brick_breaker_after_stock_setup:
    /* Pin 18 is command/data (bank 1 bit 2); pin 22 is panel reset
     * (bank 1 bit 6). Match stock my_app entry setup and reset timing. */
    r6 = 0x001e5040
    r3 = 0x001e0200
    r0 = [r3+0]
    [r6+8] &= 0xfffffffb
    r0 = [r3+0]
    [r6+12] |= 0x4
    r0 = [r3+0]
    [r6+8] &= 0xffffffbf
    r0 = [r3+0]
    [r6+12] |= 0x40
    r0 = [r3+0]
    [r6+0] &= 0xffffffbf
    r3 = 0x02000a00
    r0 = 20
    call r3
    r3 = 0x001e0200
    r0 = [r3+0]
    [r6+0] |= 0x40
    r3 = 0x02000a00
    r0 = 20
    call r3
    r3 = 0x02000a00
    r0 = 100
    call r3

    r0 = 0x11
    call hello_command
    r3 = 0x02000a00
    r0 = 120
    call r3
    r8 = panel_init_table
    r9 = 15
panel_command_loop:
    r0 = b[r8+0] (u)
    r8 += 1
    call hello_command
    r10 = b[r8+0] (u)
    r8 += 1
    if (r10 == 0) goto panel_command_done
panel_data_loop:
    r0 = b[r8+0] (u)
    r8 += 1
    call hello_data_byte
    r10 -= 1
    if (r10 != 0) goto panel_data_loop
panel_command_done:
    r9 -= 1
    if (r9 != 0) goto panel_command_loop

    /* LCD buffer plus old paddle position and a 30-brick bitset. */
    sp += -136
    r8 = sp + 0
    call clear_screen

    /* Recreate the stock PWM descriptor at RAM 0x12208, including the saved
     * brightness formula and corrected parallel-issued +20 value. */
    r0 = 0x12208
    r1 = 1000
    [r0+0] = r1
    r1 = 2
    [r0+4] = r1
    [r0+8] = r1
    r1 = 0x12baa
    r1 = b[r1+0] (u)
    r1 = r1 * 0x7d0
    r1 = r1 + 2000
    h[r0+12] = r1
    r1 = 0
    [r0+16] = r1
    r1 = 0xff14
    h[r0+20] = r1
    r1 = 1
    b[r0+24] = r1
    r3 = 0x01e79dc6
    call r3

ball_start:
    /* Persistent state: ball x/y, dx/dy, paddle x, key address, score. */
    r9 = 64
    r10 = 80
    r11 = 2
    r12 = 2
    r13 = 64
    r14 = 0x0000c3af
    r15 = 0
    r0 = 0x3fffffff
    [r8+132] = r0
    call draw_bricks
    r0 = 136
    r1 = 0
    call draw_score
    r0 = r13
    r1 = 112
    r2 = 32
    r3 = 4
    r4 = 0xffffffff
    r5 = 2
    call draw_rect
    r0 = r9
    r1 = r10
    r2 = 8
    r3 = 8
    r4 = 0x00f800f8
    r5 = 1
    call draw_rect

ball_loop:
    r0 = 20
    r3 = 0x01e516c6
    call r3
    r3 = 0x01e515ac
    call r3
    [r8+128] = r13
    r6 = r9
    r7 = r10
    r0 = b[r14+0] (u)
    if (r0 == 12) goto paddle_left
    if (r0 == 15) goto paddle_right
    goto ball_advance
paddle_left:
    if (r13 == 0) goto ball_advance
    r13 -= 4
    goto ball_advance
paddle_right:
    if (r13 == 128) goto ball_advance
    r13 += 4

ball_advance:
    r9 += r11
    if (r9 <= 0) goto ball_left_wall
    if (r9 >= 152) goto ball_right_wall
    goto ball_y_advance
ball_left_wall:
    r9 = 0
    r11 = 2
    goto ball_y_advance
ball_right_wall:
    r9 = 152
    r11 = -2
ball_y_advance:
    r10 += r12
    if (r10 <= 8) goto ball_top_wall
    goto ball_paddle_check
ball_top_wall:
    r10 = 8
    r12 = 2
ball_paddle_check:
    if (r12 != 2) goto ball_brick_check
    if (r10 < 106) goto ball_brick_check
    r0 = r9 + 8
    if (r0 <= r13) goto ball_brick_check
    r0 = r13 + 32
    if (r9 >= r0) goto ball_brick_check
    r10 = 104
    r12 = -2

ball_brick_check:
    if (r12 != -2) goto ball_miss_check
    /* Check the ball's leading top edge before it can erase a brick. */
    r0 = r10
    if (r0 < 16) goto ball_miss_check
    if (r0 >= 46) goto ball_miss_check
    r0 -= 16
    r2 = 10
    r3 = r0 / r2 (u)
    r1 = r3 * 10
    r0 = r0 - r1
    if (r0 >= 9) goto ball_miss_check
    if (r9 == 0) goto ball_miss_check
    r0 = r9 - 1
    r2 = 15
    r1 = r0 / r2 (u)
    if (r1 >= 10) goto ball_miss_check
    r0 = r3 * 10
    r0 += r1
    r2 = 1
    r2 = r2 << r0
    r0 = [r8+132]
    r1 = r0 & r2
    if (r1 == 0) goto ball_miss_check
    r0 = r0 ^ r2
    [r8+132] = r0
    r12 = 2
    r15 += 1
    /* Remove exactly the hit brick and redraw the score. */
    r0 = r9 - 1
    r2 = 15
    r0 = r0 / r2 (u)
    r0 = r0 * 15
    r0 += 8
    r1 = r10
    r1 -= 16
    r2 = 10
    r1 = r1 / r2 (u)
    r1 = r1 * 10
    r1 += 16
    r2 = 8
    r3 = 8
    r4 = 0
    r5 = 1
    call draw_rect
    r0 = 136
    r1 = 0
    call draw_score
    r0 = [r8+132]
    if (r0 == 0) goto game_over

ball_miss_check:
    if (r10 >= 120) goto game_over
ball_redraw:
    r0 = r6
    r1 = r7
    r2 = 8
    r3 = 8
    r4 = 0
    r5 = 1
    call draw_rect
    r0 = [r8+128]
    if (r13 == r0) goto ball_draw_paddle
    r1 = 112
    r2 = 32
    r3 = 4
    r4 = 0
    r5 = 2
    call draw_rect
ball_draw_paddle:
    r0 = r13
    r1 = 112
    r2 = 32
    r3 = 4
    r4 = 0xffffffff
    r5 = 2
    call draw_rect
    r0 = r9
    r1 = r10
    r2 = 8
    r3 = 8
    r4 = 0x00f800f8
    r5 = 1
    call draw_rect
    goto ball_loop

game_over:
    call clear_screen
    r6 = 44
    r7 = game_over_text
    r5 = 9
game_over_letters:
    r2 = b[r7+0] (u)
    r2 = r2 << 3
    r3 = font_glyphs
    r2 += r3
    r0 = r6
    r1 = 56
    call draw_glyph
    r6 += 8
    r7 += 1
    r5 -= 1
    if (r5 != 0) goto game_over_letters
    r0 = 72
    r1 = 72
    call draw_score
game_over_wait:
    r3 = 0x01e515ac
    call r3
    r0 = b[r14+0] (u)
    if (r0 == 13) goto game_restart
    r0 = 20
    r3 = 0x01e516c6
    call r3
    goto game_over_wait
game_restart:
    call clear_screen
    goto ball_start

draw_bricks:
    [--sp] = {rets, r7-r4}
    r6 = 0
    r7 = 0
brick_loop:
    r0 = r7 * 15
    r0 += 8
    r1 = r6 * 10
    r1 += 16
    r2 = 8
    r3 = 8
    r4 = 0xe007e007
    r5 = 1
    call draw_rect
    r7 += 1
    if (r7 != 10) goto brick_loop
    r7 = 0
    r6 += 1
    if (r6 != 3) goto brick_loop
    {pc, r7-r4} = [sp++]

draw_score:
    [--sp] = {rets, r7-r4}
    r4 = r0
    r5 = r1
    r0 = 10
    r6 = r15 / r0 (u)
    r7 = r6 * 10
    r7 = r15 - r7
    r2 = r6 << 3
    r3 = font_glyphs
    r2 += r3
    r0 = r4
    r1 = r5
    call draw_glyph
    r2 = r7 << 3
    r3 = font_glyphs
    r2 += r3
    r0 = r4 + 8
    r1 = r5
    call draw_glyph
    {pc, r7-r4} = [sp++]

/* r0=x, r1=y, r2=pointer to eight rows of 8-bit bitmap. */
draw_glyph:
    [--sp] = {rets, r7-r4}
    r4 = r0
    r5 = r1
    r6 = r2
    r7 = r8
    r3 = 8
glyph_row:
    r2 = b[r6+0] (u)
    r6 += 1
    r1 = 8
glyph_pixel:
    r0 = r2 & 0x80
    if (r0 == 0) goto glyph_black
    r0 = 0xffff
glyph_black:
    h[r7+0] = r0
    r7 += 2
    r2 = r2 << 1
    r1 -= 1
    if (r1 != 0) goto glyph_pixel
    r3 -= 1
    if (r3 != 0) goto glyph_row
    r0 = r4
    r1 = r5
    r2 = r4 + 7
    r3 = r5 + 7
    call hello_set_window
    r3 = 0x02001634
    call r3
    r0 = 1
    r1 = r8
    r2 = 128
    r3 = hello_transfer_submit
    call r3
    r3 = 0x02001650
    call r3
    {pc, r7-r4} = [sp++]

/* Digits 0-9, then G A M E O V R and a blank. */
font_glyphs:
    .byte 0x3c,0x66,0x6e,0x76,0x66,0x66,0x3c,0
    .byte 0x18,0x38,0x18,0x18,0x18,0x18,0x3c,0
    .byte 0x3c,0x66,0x06,0x0c,0x30,0x60,0x7e,0
    .byte 0x3c,0x66,0x06,0x1c,0x06,0x66,0x3c,0
    .byte 0x0c,0x1c,0x3c,0x6c,0x7e,0x0c,0x0c,0
    .byte 0x7e,0x60,0x7c,0x06,0x06,0x66,0x3c,0
    .byte 0x1c,0x30,0x60,0x7c,0x66,0x66,0x3c,0
    .byte 0x7e,0x66,0x0c,0x18,0x18,0x18,0x18,0
    .byte 0x3c,0x66,0x66,0x3c,0x66,0x66,0x3c,0
    .byte 0x3c,0x66,0x66,0x3e,0x06,0x0c,0x38,0
    .byte 0x3c,0x66,0x60,0x6e,0x66,0x66,0x3c,0
    .byte 0x18,0x3c,0x66,0x66,0x7e,0x66,0x66,0
    .byte 0x63,0x77,0x7f,0x6b,0x63,0x63,0x63,0
    .byte 0x7e,0x60,0x60,0x7c,0x60,0x60,0x7e,0
    .byte 0x3c,0x66,0x66,0x66,0x66,0x66,0x3c,0
    .byte 0x66,0x66,0x66,0x66,0x66,0x3c,0x18,0
    .byte 0x7c,0x66,0x66,0x7c,0x78,0x6c,0x66,0
    .byte 0,0,0,0,0,0,0,0
game_over_text:
    .byte 10,11,12,13,17,14,15,13,16
    .byte 0

/* r0=x0, r1=y0, r2=width, r3=height, r4=color, r5=128-byte chunks.
 * A ball is 8x8 (one chunk); a paddle is 32x4 (two chunks). */
draw_rect:
    [--sp] = {rets, r7-r4}
    r2 = r0 + r2
    r2 -= 1
    r3 = r1 + r3
    r3 -= 1
    call hello_set_window
    r6 = r8
    r7 = 32
rect_fill_buffer:
    [r6+0] = r4
    r6 += 4
    r7 -= 1
    if (r7 != 0) goto rect_fill_buffer
    r6 = r5
    r7 = hello_transfer_submit
rect_chunks:
    r3 = 0x02001634
    call r3
    r0 = 1
    r1 = r8
    r2 = 128
    call r7
    r3 = 0x02001650
    call r3
    r6 -= 1
    if (r6 != 0) goto rect_chunks
    {pc, r7-r4} = [sp++]

clear_screen:
    [--sp] = {rets, r6-r4}
    r4 = r8
    r5 = 32
    r0 = 0
clear_zero_buffer:
    [r4+0] = r0
    r4 += 4
    r5 -= 1
    if (r5 != 0) goto clear_zero_buffer
    r0 = 0
    r1 = 0
    r2 = 159
    r3 = 127
    r6 = hello_set_window
    call r6
    r5 = 320
    r6 = hello_transfer_submit
clear_chunks:
    r3 = 0x02001634
    call r3
    r0 = 1
    r1 = r8
    r2 = 128
    call r6
    r3 = 0x02001650
    call r3
    r5 -= 1
    if (r5 != 0) goto clear_chunks
    {pc, r6-r4} = [sp++]

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

/* Exact command/data order decoded from stock my_app app+0x08821A..
 * 0x0883D2. Each record is command, data count, then data bytes. */
panel_init_table:
    .byte 0xb1, 3, 0x05, 0x3a, 0x3a
    .byte 0xb2, 3, 0x05, 0x3a, 0x3a
    .byte 0xb3, 6, 0x05, 0x3a, 0x3a, 0x05, 0x3a, 0x3a
    .byte 0xb4, 1, 0x00
    .byte 0xc0, 3, 0x62, 0x02, 0x04
    .byte 0xc1, 1, 0xc0
    .byte 0xc2, 2, 0x0d, 0x00
    .byte 0xc3, 2, 0x8d, 0x6a
    .byte 0xc4, 2, 0x8d, 0xee
    .byte 0xc5, 1, 0x11
    .byte 0x36, 1, 0x70
    .byte 0xe0, 16, 0x03, 0x1b, 0x12, 0x11, 0x3f, 0x3a, 0x32, 0x34
    .byte 0x2f, 0x2b, 0x30, 0x3a, 0x00, 0x01, 0x02, 0x05
    .byte 0xe1, 16, 0x03, 0x1b, 0x12, 0x11, 0x32, 0x2f, 0x2a, 0x2f
    .byte 0x2e, 0x2c, 0x35, 0x3f, 0x00, 0x00, 0x01, 0x05
    .byte 0x3a, 1, 0x05
    .byte 0x29, 0

    .balign 4
