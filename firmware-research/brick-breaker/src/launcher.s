/* Observe stock Back key state and hold count at the my_app loop head.
 * Do not call the key decoder: doing so in stage 28 consumed short Back taps
 * before the stock menu handler could process them. The stock loop still runs
 * on every pass until a sustained hold is detected.
 *
 * sp+68 in the original my_app frame holds an initialization-only temporary.
 * Its last known use is the drawing call at app+0x08895A, before this hook at
 * app+0x088906. After saving rets/r4, that slot is at sp+76 here. The word
 * stores a magic upper half and the initial stock key-hold count below it.
 */
    .section .launcher,"ax",@progbits
    .globl brick_breaker_hotkey
brick_breaker_hotkey:
    [--sp] = {rets, r4}
    r3 = 0x0000c3af
    r0 = b[r3+0] (u)
    if (r0 != 15) goto back_released

    r2 = [sp+76]
    r1 = r2 >> 16
    r3 = 0xa5a5
    if (r1 != r3) goto arm_back_hold
    r3 = 0x0000c3b2
    r0 = h[r3+0] (u)
    r2 = r2.l (u)
    if (r0 < r2) goto arm_back_hold
    r0 -= r2
    if (r0 < 40) goto stock_loop_call

    /* my_app has only its original 124-byte task frame at this call site. */
    {rets, r4} = [sp++]
    sp += 124
    r3 = brick_breaker_after_stock_setup
    goto r3

arm_back_hold:
    r3 = 0x0000c3b2
    r0 = h[r3+0] (u)
    r3 = 0xa5a50000
    r0 |= r3
    [sp+76] = r0
    goto stock_loop_call

back_released:
    r0 = 0
    [sp+76] = r0

stock_loop_call:
    r3 = 0x01e58ee4
    call r3
    {pc, r4} = [sp++]
