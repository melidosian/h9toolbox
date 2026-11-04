/* First-pass APRS reset and 48-entry vector table, matching the stock count.
 * Board interrupts are not mapped yet; all default to an inert handler. */
.syntax unified
.cpu cortex-m0
.thumb

.section .isr_vector, "a", %progbits
.global h9_aprs_vectors
h9_aprs_vectors:
    .word __stack_top
    .word Reset_Handler + 1
    .rept 46
    .word Default_Handler + 1
    .endr

.section .text.Reset_Handler, "ax", %progbits
.thumb_func
.global Reset_Handler
Reset_Handler:
    bl h9_aprs_boot
1:  b 1b

.section .text.Default_Handler, "ax", %progbits
.thumb_func
.global Default_Handler
Default_Handler:
1:  b 1b
