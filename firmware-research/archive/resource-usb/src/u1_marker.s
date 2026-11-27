/* U1: prove that the BR23 mask ROM's JUMP_TO_MEMORY returns to its USB
 * command loop. Uploaded to RAM 0x12000 (where the USB flasher already
 * places its RAM helper) and entered with JUMP_TO_MEMORY.
 *
 * Writes "H9U1", the two argument registers, and a run counter to RAM
 * 0x13000..0x1300F, then returns. Uses only r2/r3 and touches no flash,
 * GPIO, or peripheral register.
 */
    .section .text,"ax",@progbits
    .globl entry
entry:
    r2 = 0x00013000
    r3 = 0x31553948
    [r2+0] = r3
    [r2+4] = r0
    [r2+8] = r1
    r3 = [r2+12]
    r3 += 1
    [r2+12] = r3
    rts
