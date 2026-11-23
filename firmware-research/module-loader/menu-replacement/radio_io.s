/* Pinned v1.0.38 partition I/O. Preserve the full stock register set at the
 * boundary, as the module's existing stock-call helpers do. Both return zero
 * on success. Offsets exclude the eight-byte programmer identity header. */
    .section .text,"ax",@progbits
    .globl radio_partition_read
radio_partition_read:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e52cb2
    call r4
    {pc, r14-r4} = [sp++]

    .globl radio_vm_read
radio_vm_read:
    [--sp] = {rets, r14-r4}
    /* Direct VM read preserves the missing-record status. The syscfg
     * dispatcher replaces it with the result of a read-only fallback. */
    r4 = 0x01e6285e
    call r4
    {pc, r14-r4} = [sp++]

    .globl radio_vm_write
radio_vm_write:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e62b56
    call r4
    {pc, r14-r4} = [sp++]
    .globl radio_partition_write
radio_partition_write:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e50128
    call r4
    {pc, r14-r4} = [sp++]
