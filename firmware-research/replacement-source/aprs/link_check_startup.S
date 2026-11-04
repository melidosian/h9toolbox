/* First-pass APRS reset and 48-entry vector table, matching the stock count.
 * Stock slots 15, 25, 28, 43, and 44 dispatch SysTick, DMA1, ADC1,
 * USART1, and USART2. */
.syntax unified
.cpu cortex-m0
.thumb

.section .isr_vector, "a", %progbits
.global h9_aprs_vectors
h9_aprs_vectors:
    .word __stack_top
    .word Reset_Handler + 1
    .rept 13
    .word Default_Handler + 1
    .endr
    .word h9_aprs_systick_handler + 1
    .rept 9
    .word Default_Handler + 1
    .endr
    .word h9_aprs_dma1_channel1_handler + 1
    .word h9_aprs_dma1_channel3_handler + 1
    .rept 1
    .word Default_Handler + 1
    .endr
    .word h9_aprs_adc1_handler + 1
    .rept 14
    .word Default_Handler + 1
    .endr
    .word h9_aprs_usart1_handler + 1
    .word h9_aprs_usart2_handler + 1
    .rept 3
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
