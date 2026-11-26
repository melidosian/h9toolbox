/* The native Factory R handler reloads with this exact sequence at app74A4E.
 * It can run in Menu context without resetting the CPU; guard the complete
 * call with the resident stock_tx reservation. */
    .section .text,"ax",@progbits
    .globl radio_system_reload_native
radio_system_reload_native:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e7198c
    call r4
    r4 = 0x01e71e30
    call r4
    r4 = 0x01e72b2c
    call r4
    r4 = 0x01e50b7c
    call r4
    r0 = 0
    {pc, r14-r4} = [sp++]

    .globl radio_system_retune_native
radio_system_retune_native:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e58e3e
    r0 = 0
    call r4
    {pc, r14-r4} = [sp++]

    .globl radio_system_factory_native
radio_system_factory_native:
    [--sp] = {rets, r14-r4}
    if (r0 == 0) goto reset_vfo
    if (r0 == 1) goto reset_channels
    if (r0 == 2) goto reset_analog
    if (r0 == 3) goto reset_all
    if (r0 == 4) goto reset_aprs
    r0 = 2
    {pc, r14-r4} = [sp++]
reset_vfo:
    r4 = 0x01e70854
    goto reset_call
reset_channels:
    r4 = 0x01e70a1e
    goto reset_call
reset_analog:
    r4 = 0x01e70d70
    goto reset_call
reset_all:
    r4 = 0x01e7191a
    goto reset_call
reset_aprs:
    r4 = 0x01e715f2
reset_call:
    call r4
    r0 = 0
    {pc, r14-r4} = [sp++]

    .globl radio_system_header_native
radio_system_header_native:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e67910
    call r4
    {pc, r14-r4} = [sp++]

    .globl radio_system_uart_native
radio_system_uart_native:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e5b4aa
    call r4
    {pc, r14-r4} = [sp++]
