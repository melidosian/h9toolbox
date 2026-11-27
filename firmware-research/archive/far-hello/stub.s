/* Far launcher stub, 24 bytes in the spare tail of the H9 Toolbox .tables
 * cave (app+0xA7E80). The Brick Breaker Back-hold launcher jumps here instead
 * of into the game. If the far region (flash 0xEA000, mapped at 0x01EE5000)
 * starts with the "H9X1" magic as the CPU sees it, enter the far program;
 * otherwise start Brick Breaker exactly as before. */
    .section .text,"ax",@progbits
    .globl far_stub
far_stub:
    r3 = 0x01ee5000
    r0 = [r3+0]
    r1 = 0x31583948
    if (r0 != r1) goto brick_breaker_after_stock_setup
    r3 += 8
    goto r3
