#ifndef H9_APRS_RUNTIME_STATE_H
#define H9_APRS_RUNTIME_STATE_H

#include "h9_aprs_controller.h"
#include "h9_aprs_tx_service.h"
#include "h9_aprs_uart1_board.h"
#include "h9_aprs_uart1_dma.h"
#include "h9_aprs_uart_receive.h"

/* One instance is reserved in the diagnostic board entry so the linker
 * measures the RAM required by the current receive, decode, and TX source. */
typedef struct {
    H9AprsController controller;
    H9AprsTxService tx_service;
    H9AprsUartReceive uart_receive;
    H9AprsUart1Dma uart1_dma;
    H9AprsUart1Board uart1_board;
} H9AprsRuntimeState;

#endif
