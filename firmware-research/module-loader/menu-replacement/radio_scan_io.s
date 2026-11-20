/* Pinned factory v1.0.38 stock RF backends. CPU = app offset + 0x01e00100.
 * The boundary preserves the complete module ABI register set. */
    .section .text,"ax",@progbits
    .globl radio_scan_read_reg
radio_scan_read_reg:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e505fe
    call r4
    {pc, r14-r4} = [sp++]
    .globl radio_scan_write_reg
radio_scan_write_reg:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e5070a
    call r4
    {pc, r14-r4} = [sp++]
    .globl radio_scan_retune
radio_scan_retune:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e595b6
    call r4
    {pc, r14-r4} = [sp++]
    .globl radio_scan_filter
radio_scan_filter:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e593f2
    call r4
    {pc, r14-r4} = [sp++]
    .globl radio_scan_internal_spk
radio_scan_internal_spk:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e50820
    call r4
    {pc, r14-r4} = [sp++]
