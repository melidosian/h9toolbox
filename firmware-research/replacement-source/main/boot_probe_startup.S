/* Minimal Pi32v2 boot probe. Keep the stock argument-copy and stack setup
 * sequence so the loader sees the same launch ABI as the v1.0.38 app. */
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
