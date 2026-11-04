#ifndef H9_APRS_RUNTIME_STATE_H
#define H9_APRS_RUNTIME_STATE_H

#include "h9_aprs_adc_board.h"
#include "h9_aprs_controller.h"
#include "h9_aprs_gpio_board.h"
#include "h9_aprs_sample_timer.h"
#include "h9_aprs_tx_service.h"
#include "h9_aprs_uart1_board.h"
#include "h9_aprs_uart1_dma.h"
#include "h9_aprs_uart1_tx.h"
#include "h9_aprs_uart_receive.h"
#include "h9_aprs_wave_output.h"

/* One instance is reserved in the diagnostic board entry so the linker
 * measures the RAM required by the current receive, decode, and TX source. */
typedef struct {
    H9AprsController controller;
    H9AprsTxService tx_service;
    H9AprsUartReceive uart_receive;
    H9AprsUart1Dma uart1_dma;
    H9AprsUart1Board uart1_board;
    H9AprsUart1Tx uart1_tx;
    H9AprsGpioBoard gpio_board;
    H9AprsWaveOutput wave_output;
    H9AprsAdcBoard adc_board;
    H9AprsSampleTimer sample_timer;
} H9AprsRuntimeState;

#endif
