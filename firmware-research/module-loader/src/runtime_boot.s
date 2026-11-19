/* Runs from the former game launcher, after stock my_app initialization.
 * Never enter the far service until its exact compiled body is present. */
    .section .text,"ax",@progbits
    .globl ml_runtime_boot
ml_runtime_boot:
    [--sp] = {rets, r4}
    r4 = 0x29a00
    r0 = b[r4+5] (u)
    if (r0 != 0) goto ml_runtime_done
    r0 = 0x12234
    r0 = h[r0+0] (u)
    r1 = 0x5aa5
    if (r0 != r1) goto ml_runtime_done
    r0 = 1
    b[r4+5] = r0
    r0 = RUNTIME_ENTRY
    r1 = RUNTIME_LENGTH
    r3 = LOADER_CRC
    call r3
    r1 = RUNTIME_CRC
    if (r0 != r1) goto ml_runtime_done
    r3 = RUNTIME_ENTRY
    call r3
    [r4+0] = r0
ml_runtime_done:
    {pc, r4} = [sp++]
