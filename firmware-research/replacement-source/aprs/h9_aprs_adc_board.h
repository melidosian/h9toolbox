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
    volatile uint32_t *nvic_iser;
    /* Counterparts of the stock ADC driver's state at +0x38/+0x3C.
     * The replacement keeps these separately from its register pointers. */
    uint32_t irq_flags;
    uint32_t event_flags;
    uint32_t state_word_28;
    uint8_t state_byte_19;
} H9AprsAdcBoard;

void h9_aprs_adc_board_bind(H9AprsAdcBoard *board,
                             volatile uint32_t *rcc,
                             volatile uint32_t *gpio_a,
                             volatile uint32_t *adc,
                             volatile uint32_t *nvic_iser);
void h9_aprs_adc_board_bind_target(H9AprsAdcBoard *board);
/* First half of stock 0x080026D4: pulse ADC reset, then enable its clock. */
bool h9_aprs_adc_board_reset_enable(H9AprsAdcBoard *board);
/* Stock 0x08002CAC calibration with bounded polling. poll_step is optional
 * and lets a host test emulate the hardware clearing the calibration bit. */
bool h9_aprs_adc_board_calibrate(H9AprsAdcBoard *board,
                                 uint32_t poll_limit,
                                 void (*poll_step)(void *context),
                                 void *context);
/* Stock 0x08003244 enables ADC/GPIOA clocks and sets PA1 analog. */
void h9_aprs_adc_board_prepare_pin(H9AprsAdcBoard *board);
/* Ported register configuration from 0x080030A0 and channel choice from
 * 0x08002D6C. Rejects an already-running conversion. */
bool h9_aprs_adc_board_configure(H9AprsAdcBoard *board);
/* Port of the 0x080032C0 enable/start path. Replacement waits for ADC ready
 * instead of using the stock fixed delay. */
bool h9_aprs_adc_board_start(H9AprsAdcBoard *board, uint32_t poll_limit,
                             void (*poll_step)(void *context), void *context);
uint16_t h9_aprs_adc_board_read(const H9AprsAdcBoard *board);
/* Sample-producing branch of the stock IRQ 12 handler at 0x08002F8A. */
bool h9_aprs_adc_board_sample_irq(H9AprsAdcBoard *board,
                                  H9AprsController *controller);
/* Full observed IRQ 12 dispatch: sample (0xC), watchdog (0x80), and
 * end-of-sequence (0x10). Returns the handled flag mask. The two stock
 * branch callbacks are one-instruction returns; state/flag effects remain. */
uint32_t h9_aprs_adc_board_irq(H9AprsAdcBoard *board,
                                H9AprsController *controller);

#endif
