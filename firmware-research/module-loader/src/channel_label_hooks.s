/* The main loop checks the exact far-menu payload before any display or
 * clone request can jump into it. The state is reset by ml_aprs_boot. */
    .section .text,"ax",@progbits
    .globl ml_labels_tick
ml_labels_tick:
    [--sp] = {rets, r4}
    r4 = 0x29900
    r0 = b[r4+0] (u)
    if (r0 == 2) goto labels_paint
    if (r0 != 0) goto labels_done
    r0 = 1
    b[r4+0] = r0
    r0 = LABEL_FAR_BASE
    r1 = LABEL_FAR_LENGTH
    r3 = LABEL_CRC_HELPER
    call r3
    r1 = LABEL_FAR_CRC
    if (r0 != r1) goto labels_done
    r0 = 2
    b[r4+0] = r0
labels_paint:
    r3 = LABEL_FAR_TICK
    call r3
labels_done:
    {pc, r4} = [sp++]

    .globl ml_labels_draw_a
ml_labels_draw_a:
    r3 = 0x29900
    r3 = b[r3+0] (u)
    if (r3 != 2) goto labels_stock_a
    r3 = LABEL_FAR_DRAW_A
    goto r3
labels_stock_a:
    r3 = 0x01e5a486
    goto r3

    .globl ml_labels_draw_b
ml_labels_draw_b:
    r3 = 0x29900
    r3 = b[r3+0] (u)
    if (r3 != 2) goto labels_stock_b
    r3 = LABEL_FAR_DRAW_B
    goto r3
labels_stock_b:
    r3 = 0x01e5a54e
    goto r3

/* Read receives partition, offset, byte output pointer and length. Keep all
 * original args for the stock S2 service outside the three reserved blocks. */
    .globl ml_labels_read
ml_labels_read:
    [--sp] = {rets, r4}
    r4 = 0x3f00
    if (r1 < r4) goto labels_read_stock
    r4 = 0x3f60
    if (r1 >= r4) goto labels_read_stock
    r4 = 0x29900
    r4 = b[r4+0] (u)
    if (r4 != 2) goto labels_read_stock
    r0 = r1
    r1 = r2
    r4 = LABEL_FAR_READ
    call r4
    {pc, r4} = [sp++]
labels_read_stock:
    r4 = 0x01ea52a8
    call r4
    {pc, r4} = [sp++]

/* Stock W has address in r6 and callback in r7. Only pass W to stock outside
 * the name range, even if the far payload failed verification. */
    .globl ml_labels_write
ml_labels_write:
    [--sp] = {rets, r4}
    r4 = 0x3f00
    if (r6 < r4) goto labels_write_stock
    r4 = 0x3f60
    if (r6 >= r4) goto labels_write_stock
    r4 = 0x29900
    r4 = b[r4+0] (u)
    if (r4 != 2) goto labels_write_done
    r4 = 0x3f20
    if (r6 == r4) goto labels_write_far
    r4 = 0x3f40
    if (r6 != r4) goto labels_write_done
labels_write_far:
    r0 = r6
    r4 = LABEL_FAR_WRITE
    call r4
labels_write_done:
    r0 = 0
    {pc, r4} = [sp++]
labels_write_stock:
    r4 = 0x01ea5300
    call r4
    {pc, r4} = [sp++]

/* Classic uses all four register arguments and a fifth stack argument for
 * color. Forward that stack word on both the stock and the far branch. */
    .globl ml_labels_draw_classic
ml_labels_draw_classic:
    [--sp] = {rets, r4}
    r4 = [sp+8]
    sp += -4
    [sp] = r4
    r4 = 0x29900
    r4 = b[r4+0] (u)
    if (r4 != 2) goto labels_stock_classic
    r4 = LABEL_FAR_DRAW_CLASSIC
    goto labels_classic_call
labels_stock_classic:
    r4 = 0x01e59a08
labels_classic_call:
    call r4
    sp += 4
    {pc, r4} = [sp++]

/* The TX frequency renderer also has five arguments. Preserve its original
 * font argument if the far payload is unavailable. */
    .globl ml_labels_draw_tx_frequency
ml_labels_draw_tx_frequency:
    [--sp] = {rets, r4}
    r4 = [sp+8]
    sp += -4
    [sp] = r4
    r4 = 0x29900
    r4 = b[r4+0] (u)
    if (r4 != 2) goto labels_stock_tx_frequency
    r4 = LABEL_FAR_DRAW_TX_FREQUENCY
    goto labels_tx_frequency_call
labels_stock_tx_frequency:
    r4 = 0x01e5977c
labels_tx_frequency_call:
    call r4
    sp += 4
    {pc, r4} = [sp++]
