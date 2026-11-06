/* Stage twenty-two minimal Snake: stock boot and key decode, authored game.
 * Four direction buttons were hardware verified in stage twenty-one.
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

    /* Keep 128 bytes for LCD transfers and 32 bytes for snake segments. */
    sp += -160
    r8 = sp + 0
    call snake_clear_screen

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

snake_start:
    /* Ten columns by eight rows. Body entries are 0..79 cell numbers. */
    r6 = r8 + 128
    r9 = 45
    r11 = 3
    r12 = 3
    r14 = 47
    r15 = -1
    r10 = 0x0000c3af
    r0 = 45
    b[r6+0] = r0
    r0 = 44
    b[r6+1] = r0
    r0 = 43
    b[r6+2] = r0
    r3 = 0x01e515ac
    call r3
    r13 = b[r10+0] (u)
    r0 = 43
    r1 = 0xe007e007
    call snake_draw_cell
    r0 = 44
    r1 = 0xe007e007
    call snake_draw_cell
    r0 = 45
    r1 = 0xe007e007
    call snake_draw_cell
    r0 = 47
    r1 = 0x00f800f8
    call snake_draw_cell

snake_poll:
    r3 = 0x01e515ac
    call r3
    r0 = b[r10+0] (u)
    if (r0 == 0) goto snake_key_released
    if (r13 != 0) goto snake_poll_delay
    r13 = 1
    if (r0 == 13) goto snake_turn_up
    if (r0 == 14) goto snake_turn_down
    if (r0 == 12) goto snake_turn_left
    if (r0 == 15) goto snake_turn_right
    goto snake_poll_delay
snake_key_released:
    r13 = 0
    goto snake_poll_delay
snake_turn_up:
    if (r12 == 1) goto snake_poll_delay
    r12 = 0
    r15 = 0
    goto snake_poll_delay
snake_turn_down:
    if (r12 == 0) goto snake_poll_delay
    r12 = 1
    r15 = 0
    goto snake_poll_delay
snake_turn_left:
    if (r12 == 3) goto snake_poll_delay
    r12 = 2
    r15 = 0
    goto snake_poll_delay
snake_turn_right:
    if (r12 == 2) goto snake_poll_delay
    r12 = 3
    r15 = 0
snake_poll_delay:
    r0 = 10
    r3 = 0x01e516c6
    call r3
    if (r15 == -1) goto snake_poll
    r15 += 1
    if (r15 != 40) goto snake_poll
    r15 = 0

snake_step:
    if (r12 == 0) goto snake_step_up
    if (r12 == 1) goto snake_step_down
    r2 = 10
    r0 = r9 / r2 (u)
    r1 = r0 * 10
    r1 = r9 - r1
    if (r12 == 2) goto snake_step_left
    if (r1 == 9) goto snake_game_over
    r7 = r9 + 1
    goto snake_check_body
snake_step_left:
    if (r1 == 0) goto snake_game_over
    r7 = r9 - 1
    goto snake_check_body
snake_step_up:
    if (r9 < 10) goto snake_game_over
    r7 = r9 - 10
    goto snake_check_body
snake_step_down:
    if (r9 >= 70) goto snake_game_over
    r7 = r9 + 10

snake_check_body:
    r6 = r8 + 128
    r5 = 0
    /* The tail moves away on this step unless the food cell is reached;
     * food is never placed on the body, so excluding the tail is safe. */
    r4 = r11 - 1
snake_body_scan:
    r0 = b[r6+r5] (u)
    if (r0 == r7) goto snake_game_over
    r5 += 1
    if (r5 != r4) goto snake_body_scan
    r15 = 0
    if (r7 == r14) goto snake_eat_food
snake_remove_tail:
    r4 = r11 - 1
    r0 = b[r6+r4] (u)
    r1 = 0
    call snake_draw_cell
    goto snake_shift_body
snake_eat_food:
    r15 = 1
    if (r11 == 32) goto snake_remove_tail
    r11 += 1
snake_shift_body:
    r5 = r11 - 1
snake_shift_loop:
    r4 = r5 - 1
    r0 = b[r6+r4] (u)
    b[r6+r5] = r0
    r5 -= 1
    if (r5 != 0) goto snake_shift_loop
    b[r6+0] = r7
    r9 = r7
    r0 = r7
    r1 = 0xe007e007
    call snake_draw_cell
    if (r15 == 0) goto snake_poll
snake_next_food:
    r14 += 23
    if (r14 < 80) goto snake_food_check
    r14 -= 80
snake_food_check:
    r5 = 0
snake_food_scan:
    r0 = b[r6+r5] (u)
    if (r0 == r14) goto snake_food_occupied
    r5 += 1
    if (r5 != r11) goto snake_food_scan
    r0 = r14
    r1 = 0x00f800f8
    call snake_draw_cell
    r15 = 0
    goto snake_poll
snake_food_occupied:
    r14 += 1
    if (r14 != 80) goto snake_food_check
    r14 = 0
    goto snake_food_check

snake_game_over:
    r7 = 0x001e5000
    r0 = 3
    call led_pattern
    call snake_clear_screen
    goto snake_start

/* r0 = cell 0..79, r1 = repeated big-endian RGB565 word. */
snake_draw_cell:
    [--sp] = {rets, r7-r4}
    r7 = r0
    r6 = r1
    r5 = r8
    r4 = 32
snake_color_buffer:
    [r5+0] = r6
    r5 += 4
    r4 -= 1
    if (r4 != 0) goto snake_color_buffer
    r2 = 10
    r0 = r7 / r2 (u)
    r1 = r0 * 10
    r1 = r7 - r1
    r1 = r1 << 4
    r0 = r0 << 4
    r2 = r1 + 15
    r3 = r0 + 15
    r4 = hello_set_window
    call r4
    r5 = 4
    r6 = hello_transfer_submit
snake_cell_chunks:
    r3 = 0x02001634
    call r3
    r0 = 1
    r1 = r8
    r2 = 128
    call r6
    r3 = 0x02001650
    call r3
    r5 -= 1
    if (r5 != 0) goto snake_cell_chunks
    {pc, r7-r4} = [sp++]

snake_clear_screen:
    [--sp] = {rets, r6-r4}
    r4 = r8
    r5 = 32
    r0 = 0
snake_zero_buffer:
    [r4+0] = r0
    r4 += 4
    r5 -= 1
    if (r5 != 0) goto snake_zero_buffer
    r0 = 0
    r1 = 0
    r2 = 159
    r3 = 127
    r6 = hello_set_window
    call r6
    r5 = 320
    r6 = hello_transfer_submit
snake_clear_chunks:
    r3 = 0x02001634
    call r3
    r0 = 1
    r1 = r8
    r2 = 128
    call r6
    r3 = 0x02001650
    call r3
    r5 -= 1
    if (r5 != 0) goto snake_clear_chunks
    {pc, r6-r4} = [sp++]

led_pattern:
    [--sp] = {rets, r5, r4}
    r4 = r0
led_pulse:
    r1 = [r7+0]
    r1 = r1 | 0x200
    [r7+0] = r1
    r5 = 12000000
led_on_delay:
    r5 -= 1
    if (r5 != 0) goto led_on_delay
    r1 = [r7+0]
    r1 = r1 & ~0x200
    [r7+0] = r1
    r5 = 12000000
led_off_delay:
    r5 -= 1
    if (r5 != 0) goto led_off_delay
    r4 -= 1
    if (r4 != 0) goto led_pulse
    r5 = 20000000
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
