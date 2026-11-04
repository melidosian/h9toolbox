/* Link-check vectors only. This reset handler does not initialize the board. */
.syntax unified
.cpu cortex-m0
.thumb

.section .isr_vector, "a", %progbits
.global h9_link_check_vectors
h9_link_check_vectors:
    .word __stack_top
    .word Reset_Handler + 1
    .rept 14
    .word Default_Handler + 1
    .endr
