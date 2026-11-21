/* Stock call-site shims. The prefix is populated only after the startup
 * verifier accepts the matching resident body. r0/r1 carry stock arguments. */
    .section .text,"ax",@progbits

    .macro resident_tx_method field, fallback
    r3 = 0x29a00
    r3 = [r3+0]
    r2 = RESIDENT_TX_API
    if (r3 != r2) goto \fallback
    r3 = [r3+\field]
    .endm

    .globl ml_auto_timed
ml_auto_timed:
    resident_tx_method 12, timed_stock
    [--sp] = rets
    call r3
    {rets} = [sp++]
timed_stock:
    r3 = 0x01E82FB2
    goto r3

    .globl ml_auto_prepare
ml_auto_prepare:
    resident_tx_method 16, prepare_stock
    goto r3
prepare_stock:
    r3 = 0x01E5AD68
    goto r3

    .globl ml_auto_emit
ml_auto_emit:
    resident_tx_method 20, emit_stock
    goto r3
emit_stock:
    r3 = 0x01E5BF2C
    goto r3

    .globl ml_auto_airtime
ml_auto_airtime:
    resident_tx_method 24, airtime_stock
    goto r3
airtime_stock:
    r3 = 0x01E5C36C
    goto r3

    .macro guarded_stock stock, fallback
    resident_tx_method 28, \fallback
    r2 = \stock
    goto r3
\fallback:
    r3 = \stock
    goto r3
    .endm

    .globl ml_guarded_kiss
ml_guarded_kiss:
    guarded_stock 0x01E5DBCC, kiss_stock

    .globl ml_guarded_ptt
ml_guarded_ptt:
    guarded_stock 0x01E5C54C, ptt_stock

    .globl ml_guarded_end
ml_guarded_end:
    guarded_stock 0x01E5CEF8, end_stock
