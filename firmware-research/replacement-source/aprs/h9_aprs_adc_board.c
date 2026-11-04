#include "h9_aprs_adc_board.h"

#include <stddef.h>

#define H9_APRS_ADC_BASE 0x40012400u
#define H9_APRS_ADC_DR_WORD (0x40u / 4u)
#define H9_APRS_ADC_ISR_WORD 0u
#define H9_APRS_ADC_IER_WORD 1u
#define H9_APRS_ADC_SAMPLE_FLAGS 0x0cu

void h9_aprs_adc_board_bind(H9AprsAdcBoard *board,
                             volatile uint32_t *adc)
{
    if (board != NULL) board->adc = adc;
}

void h9_aprs_adc_board_bind_target(H9AprsAdcBoard *board)
{
    h9_aprs_adc_board_bind(board,
        (volatile uint32_t *)(uintptr_t)H9_APRS_ADC_BASE);
}

uint16_t h9_aprs_adc_board_read(const H9AprsAdcBoard *board)
{
    if (board == NULL || board->adc == NULL) return 0u;
    return (uint16_t)board->adc[H9_APRS_ADC_DR_WORD];
}

bool h9_aprs_adc_board_sample_irq(H9AprsAdcBoard *board,
                                  H9AprsController *controller)
{
    uint32_t status;
    uint32_t enabled;
    if (board == NULL || board->adc == NULL || controller == NULL) return false;
    status = board->adc[H9_APRS_ADC_ISR_WORD];
    enabled = board->adc[H9_APRS_ADC_IER_WORD];
    if ((status & enabled & H9_APRS_ADC_SAMPLE_FLAGS) == 0u) return false;
    h9_aprs_controller_adc_tick(controller);
    board->adc[H9_APRS_ADC_ISR_WORD] = H9_APRS_ADC_SAMPLE_FLAGS;
    return true;
}
