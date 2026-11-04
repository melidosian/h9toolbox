#ifndef H9_APRS_ADC_BOARD_H
#define H9_APRS_ADC_BOARD_H

#include <stdint.h>

/* Stock APRS 0x080026D4 stores ADC base 0x40012400 in its sample state;
 * 0x08002F82 reads its data register at offset 0x40. */
typedef struct {
    volatile uint32_t *adc;
} H9AprsAdcBoard;

void h9_aprs_adc_board_bind(H9AprsAdcBoard *board,
                             volatile uint32_t *adc);
void h9_aprs_adc_board_bind_target(H9AprsAdcBoard *board);
uint16_t h9_aprs_adc_board_read(const H9AprsAdcBoard *board);

#endif
