#include "h9_aprs_adc_board.h"

#include <stddef.h>

#define H9_APRS_ADC_BASE 0x40012400u
#define H9_APRS_RCC_BASE 0x40021000u
#define H9_APRS_GPIO_A_BASE 0x50000000u
#define H9_APRS_RCC_GPIO_ENABLE_WORD (0x34u / 4u)
#define H9_APRS_RCC_ADC_ENABLE_WORD (0x40u / 4u)
#define H9_APRS_GPIO_MODER_WORD 0u
#define H9_APRS_GPIO_PUPDR_WORD 3u
#define H9_APRS_PA1_MODE_MASK (3u << 2)
#define H9_APRS_ADC_CLOCK_BIT (1u << 20)
#define H9_APRS_ADC_DR_WORD (0x40u / 4u)
#define H9_APRS_ADC_ISR_WORD 0u
#define H9_APRS_ADC_IER_WORD 1u
#define H9_APRS_ADC_SAMPLE_FLAGS 0x0cu

void h9_aprs_adc_board_bind(H9AprsAdcBoard *board,
                             volatile uint32_t *rcc,
                             volatile uint32_t *gpio_a,
                             volatile uint32_t *adc)
{
    if (board == NULL) return;
    board->rcc = rcc;
    board->gpio_a = gpio_a;
    board->adc = adc;
}

void h9_aprs_adc_board_bind_target(H9AprsAdcBoard *board)
{
    h9_aprs_adc_board_bind(board,
        (volatile uint32_t *)(uintptr_t)H9_APRS_RCC_BASE,
        (volatile uint32_t *)(uintptr_t)H9_APRS_GPIO_A_BASE,
        (volatile uint32_t *)(uintptr_t)H9_APRS_ADC_BASE);
}

void h9_aprs_adc_board_prepare_pin(H9AprsAdcBoard *board)
{
    if (board == NULL || board->rcc == NULL || board->gpio_a == NULL)
        return;
    board->rcc[H9_APRS_RCC_GPIO_ENABLE_WORD] |= 1u;
    board->rcc[H9_APRS_RCC_ADC_ENABLE_WORD] |= H9_APRS_ADC_CLOCK_BIT;
    board->gpio_a[H9_APRS_GPIO_MODER_WORD] =
        (board->gpio_a[H9_APRS_GPIO_MODER_WORD] & ~H9_APRS_PA1_MODE_MASK) |
        H9_APRS_PA1_MODE_MASK;
    board->gpio_a[H9_APRS_GPIO_PUPDR_WORD] &= ~H9_APRS_PA1_MODE_MASK;
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
