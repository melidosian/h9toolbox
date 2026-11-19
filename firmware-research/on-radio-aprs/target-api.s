    .section .text,"ax",@progbits

    .globl h9_clear
h9_clear:
    [--sp] = {rets, r14-r4}
    sp += -4
    r0 = 0
    r1 = 0
    r2 = 160
    r3 = 128
    [sp] = r0
    r4 = 0x01e50bbe
    call r4
    sp += 4
    {pc, r14-r4} = [sp++]

    .globl h9_line
h9_line:
    [--sp] = {rets, r14-r4}
    sp += -8
    r2 = r0
    r0 = 8
    r3 = 0xffff
    r4 = 8
    [sp] = r4
    r4 = 1
    [sp+4] = r4
    r4 = 0x01e4f01c
    call r4
    sp += 8
    {pc, r14-r4} = [sp++]

    .globl h9_delay
h9_delay:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e516c6
    call r4
    {pc, r14-r4} = [sp++]

    .globl h9_scan_keys
h9_scan_keys:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e524c6
    call r4
    r4 = 0x01e515ac
    call r4
    r4 = 0x01e7a004
    call r4
    {pc, r14-r4} = [sp++]

    .globl h9_jl
h9_jl:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e786ba
    call r4
    {pc, r14-r4} = [sp++]

    .globl h9_uart_write
h9_uart_write:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e5b4aa
    call r4
    {pc, r14-r4} = [sp++]

    .globl h9_launch
h9_launch:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e7a1fe
    call r4
    {pc, r14-r4} = [sp++]

    .globl h9_run_baud
h9_run_baud:
    [--sp] = {rets, r14-r4}
    r4 = 0x11cfc
    r4 = [r4]
    r4 = [r4+24]
    r0 = 115200
    call r4
    {pc, r14-r4} = [sp++]
