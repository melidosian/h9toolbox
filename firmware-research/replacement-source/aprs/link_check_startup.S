/* First-pass APRS reset and 48-entry vector table, matching the stock count.
 * Stock slots 25, 28, and 43 dispatch DMA channel 1, ADC1, and USART1. */
.syntax unified
.cpu cortex-m0
.thumb

.section .isr_vector, "a", %progbits
.global h9_aprs_vectors
h9_aprs_vectors:
    .word __stack_top
    .word Reset_Handler + 1
    .rept 23
    .word Default_Handler + 1
    .endr
    .word h9_aprs_dma1_channel1_handler + 1
    .rept 2
    .word Default_Handler + 1
    .endr
    .word h9_aprs_adc1_handler + 1
    .rept 14
    .word Default_Handler + 1
    .endr
    .word h9_aprs_usart1_handler + 1
    .rept 4
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
