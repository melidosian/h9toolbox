/* Flash-dependent modules check the loader before touching the jump table.
 * r0 is the loader API pointer and r1 is the module index. The tail jump
 * preserves both arguments and the original return address. */
    .section .text.module_flash_entry,"ax",@progbits
    .globl module_flash_entry
module_flash_entry:
    r2 = [r0+4]
    if (r2 != MODULE_FLASH_API) goto unsupported_loader
    goto module_main
unsupported_loader:
    r0 = 0
    rts

/* Keep each bridge in its own section so unused exports take no RAM. */
    .macro flash_call name, target
    .section .text.\name,"ax",@progbits
    .globl \name
\name:
    r3 = \target
    goto r3
    .endm

/* When r3 carries an argument, use two stack words to make the far jump.
 * Restore r4 and SP before the target runs, including for stack arguments. */
    .macro flash_call_r3 name, target
    .section .text.\name,"ax",@progbits
    .globl \name
\name:
    sp += -8
    [sp+0] = r4
    r4 = \target
    [sp+4] = r4
    r4 = [sp+0]
    sp += 4
    pc = [sp++]
    .endm
