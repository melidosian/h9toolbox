#ifndef H9_APRS_ADC_BOARD_H
#define H9_APRS_ADC_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "h9_aprs_controller.h"

/* Stock APRS 0x080026D4 stores ADC base 0x40012400 in its sample state;
 * 0x08002F82 reads its data register at offset 0x40. */
typedef struct {
    volatile uint32_t *rcc;
    volatile uint32_t *gpio_a;
    volatile uint32_t *adc;
} H9AprsAdcBoard;

void h9_aprs_adc_board_bind(H9AprsAdcBoard *board,
                             volatile uint32_t *rcc,
                             volatile uint32_t *gpio_a,
                             volatile uint32_t *adc);
void h9_aprs_adc_board_bind_target(H9AprsAdcBoard *board);
/* Stock 0x08003244 enables ADC/GPIOA clocks and sets PA1 analog. */
void h9_aprs_adc_board_prepare_pin(H9AprsAdcBoard *board);
uint16_t h9_aprs_adc_board_read(const H9AprsAdcBoard *board);
/* Sample-producing branch of the stock IRQ 12 handler at 0x08002F8A.
 * Other ADC interrupt conditions are not yet reconstructed. */
bool h9_aprs_adc_board_sample_irq(H9AprsAdcBoard *board,
                                  H9AprsController *controller);

#endif
