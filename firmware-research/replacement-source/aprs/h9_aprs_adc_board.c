#include "h9_aprs_adc_board.h"

#include <stddef.h>

#define H9_APRS_ADC_BASE 0x40012400u
#define H9_APRS_ADC_DR_WORD (0x40u / 4u)

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
