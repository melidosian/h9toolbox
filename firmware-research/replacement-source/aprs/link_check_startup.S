/* First-pass APRS reset and 48-entry vector table. Reserved slots and
 * exception behavior follow the pinned stock image. */
.syntax unified
.cpu cortex-m0
.thumb

.section .isr_vector, "a", %progbits
.global h9_aprs_vectors
h9_aprs_vectors:
    .word __stack_top                       /* 0 */
    .word Reset_Handler + 1                 /* 1 */
    .word h9_aprs_nmi_handler + 1           /* 2 */
    .word h9_aprs_hardfault_handler + 1     /* 3 */
    .rept 7                                 /* 4..10 */
    .word 0
    .endr
    .word h9_aprs_svc_handler + 1           /* 11 */
    .word 0                                 /* 12 */
    .word 0                                 /* 13 */
    .word h9_aprs_pendsv_handler + 1        /* 14 */
    .word h9_aprs_systick_handler + 1       /* 15 */
    .rept 8                                 /* 16..23 */
    .word Default_Handler + 1
    .endr
    .word 0                                 /* 24 */
    .word h9_aprs_dma1_channel1_handler + 1 /* 25 */
    .word h9_aprs_dma1_channel3_handler + 1 /* 26 */
    .word 0                                 /* 27 */
    .word h9_aprs_adc1_handler + 1          /* 28 */
    .word Default_Handler + 1               /* 29 */
    .word Default_Handler + 1               /* 30 */
    .word 0                                 /* 31 */
    .word Default_Handler + 1               /* 32 */
    .word Default_Handler + 1               /* 33 */
    .word 0                                 /* 34 */
    .word Default_Handler + 1               /* 35 */
    .word 0                                 /* 36 */
    .rept 3                                 /* 37..39 */
    .word Default_Handler + 1
    .endr
    .word 0                                 /* 40 */
    .word Default_Handler + 1               /* 41 */
    .word 0                                 /* 42 */
    .word h9_aprs_usart1_handler + 1        /* 43 */
    .word h9_aprs_usart2_handler + 1        /* 44 */
    .rept 3                                 /* 45..47 */
    .word 0
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

.section .text.h9_aprs_exception_handlers, "ax", %progbits
.thumb_func
.global h9_aprs_nmi_handler
h9_aprs_nmi_handler:
    bx lr

.thumb_func
.global h9_aprs_hardfault_handler
h9_aprs_hardfault_handler:
    nop
1:  b 1b

.thumb_func
.global h9_aprs_svc_handler
h9_aprs_svc_handler:
    bx lr

.thumb_func
.global h9_aprs_pendsv_handler
h9_aprs_pendsv_handler:
    bx lr
