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

/* Timer-3 ISR from stock app+0x03126C..0x0312D2. Direct ROM calls retain the
 * input registers left by the previous instruction, unlike a C callback. */
    .set h9_rom_timer_poll, 0x02001D2E
    .set h9_rom_clock_enter, 0x02001634
    .set h9_rom_clock_check, 0x02001E2C
    .set h9_rom_clock_leave, 0x02001650
    .globl h9_main_timer3_isr
    .type h9_main_timer3_isr,@function
h9_main_timer3_isr:
    [--sp] = {psr, rets, reti}
    [--sp] = {r4-r0}
    r0 = 0xC0A4
    [r0+0] += 1
    r0 = 0x10F0EC
    [r0+0] |= 0x40
    r0 = 0x1D8B0
    [r0+0] += 1
    call h9_rom_timer_poll
    if (r0 == 0) goto .Ltimer3_return
    call h9_rom_clock_enter
    csync
    call h9_rom_clock_check
    if (r0 == 0) goto .Ltimer3_leave
    r0 = 0
    r1 = 0xD5E8
    r2 = 0xD5BC
    r3 = [r1+0]
    r4 = [r2+0]
    if (r4 == r3) goto .Ltimer3_match
    swi 0
.Ltimer3_match:
    r0 = 1
.Ltimer3_leave:
    call h9_rom_clock_leave
.Ltimer3_return:
    {r4-r0} = [sp++]
    {psr, rets, reti} = [sp++]
    rti
    .size h9_main_timer3_isr, .-h9_main_timer3_isr

/* Stock RAM 0x1622 starts with these three save instructions, then enters
 * flash crash-report code and loops. This first-pass replacement preserves
 * the frame and leaves a compact RAM snapshot for debugger inspection. */
    .section .ram_late.h9_default_irq,"ax",@progbits
    .globl h9_main_default_irq
    .type h9_main_default_irq,@function
h9_main_default_irq:
    [--sp] = {ssp, usp, icfg, psr, rets, retx, rete, reti}
    [--sp] = {r15-r0}
    r0 = sp
    r1 = h9_main_exception_capture
    call r1
.Ldefault_irq_halt:
    goto .Ldefault_irq_halt
    .size h9_main_default_irq, .-h9_main_default_irq
