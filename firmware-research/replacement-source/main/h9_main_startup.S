/* First-pass Pi32v2 entry, based on stock app+0x000000 and app+0x02D82E.
 * The board initialization after C memory setup is still absent. */
    .section .text.h9_reset,"ax",@progbits
    .globl h9_main_reset
    .type h9_main_reset,@function
h9_main_reset:
    call h9_main_copy_boot_args
    sp = 58000
    ssp = 58000
    call h9_main_runtime_init
.Lhalt:
    goto .Lhalt
    .size h9_main_reset, .-h9_main_reset

    .type h9_main_copy_boot_args,@function
h9_main_copy_boot_args:
    r3_r2 = d[r0+4]
    r1 = 58016
    d[r1+16] = r3_r2
    r2 = [r0+0]
    r3 = [r2+8]
    [r1+24] = r3
    r2 = b[r2+13] (u)
    b[r1+0] = r2
    r2 = h[r0+12] (u)
    h[r1+28] = r2
    r2 = h[r0+14] (u)
    h[r1+30] = r2
    r3_r2 = d[r0+16]
    d[r1+32] = r3_r2
    rts
    .size h9_main_copy_boot_args, .-h9_main_copy_boot_args

/* Stock app+0x7E..0x8A clears ICFG bits 8 and 9 after copying RAM code.
 * This leaf is called by the diagnostic C runtime at the same stage. */
    .globl h9_main_interrupt_config
    .type h9_main_interrupt_config,@function
h9_main_interrupt_config:
    r2 = icfg
    r2 = r2 & 0xFFFFFCFF
    icfg = r2
    rts
    .size h9_main_interrupt_config, .-h9_main_interrupt_config

/* Timer-3 IRQ shell from stock app+0x03126C/0x0312CA. The C body performs
 * the register, ROM, and fault decisions while this preserves CPU state. */
    .globl h9_main_timer3_isr
    .type h9_main_timer3_isr,@function
h9_main_timer3_isr:
    [--sp] = {psr, rets, reti}
    [--sp] = {r4-r0}
    call h9_main_timer3_irq_target_body
    {r4-r0} = [sp++]
    {psr, rets, reti} = [sp++]
    rti
    .size h9_main_timer3_isr, .-h9_main_timer3_isr
