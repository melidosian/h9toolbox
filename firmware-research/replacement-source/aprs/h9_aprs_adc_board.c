#include "h9_aprs_adc_board.h"

#include <stddef.h>

#define H9_APRS_ADC_BASE 0x40012400u
#define H9_APRS_RCC_BASE 0x40021000u
#define H9_APRS_GPIO_A_BASE 0x50000000u
#define H9_APRS_NVIC_ISER_BASE 0xE000E100u
#define H9_APRS_RCC_GPIO_ENABLE_WORD (0x34u / 4u)
#define H9_APRS_RCC_ADC_RESET_WORD (0x30u / 4u)
#define H9_APRS_RCC_ADC_ENABLE_WORD (0x40u / 4u)
#define H9_APRS_GPIO_MODER_WORD 0u
#define H9_APRS_GPIO_PUPDR_WORD 3u
#define H9_APRS_PA1_MODE_MASK (3u << 2)
#define H9_APRS_ADC_CLOCK_BIT (1u << 20)
#define H9_APRS_ADC_DR_WORD (0x40u / 4u)
#define H9_APRS_ADC_ISR_WORD 0u
#define H9_APRS_ADC_IER_WORD 1u
#define H9_APRS_ADC_CR_WORD 2u
#define H9_APRS_ADC_CFGR1_WORD 3u
#define H9_APRS_ADC_CFGR2_WORD 4u
#define H9_APRS_ADC_SMPR_WORD 5u
#define H9_APRS_ADC_CHSELR_WORD 10u
#define H9_APRS_ADC_SAMPLE_FLAGS 0x0cu

void h9_aprs_adc_board_bind(H9AprsAdcBoard *board,
                             volatile uint32_t *rcc,
                             volatile uint32_t *gpio_a,
                             volatile uint32_t *adc,
                             volatile uint32_t *nvic_iser)
{
    if (board == NULL) return;
    board->rcc = rcc;
    board->gpio_a = gpio_a;
    board->adc = adc;
    board->nvic_iser = nvic_iser;
}

void h9_aprs_adc_board_bind_target(H9AprsAdcBoard *board)
{
    h9_aprs_adc_board_bind(board,
        (volatile uint32_t *)(uintptr_t)H9_APRS_RCC_BASE,
        (volatile uint32_t *)(uintptr_t)H9_APRS_GPIO_A_BASE,
        (volatile uint32_t *)(uintptr_t)H9_APRS_ADC_BASE,
        (volatile uint32_t *)(uintptr_t)H9_APRS_NVIC_ISER_BASE);
}

bool h9_aprs_adc_board_reset_enable(H9AprsAdcBoard *board)
{
    if (board == NULL || board->rcc == NULL || board->adc == NULL)
        return false;
    board->rcc[H9_APRS_RCC_ADC_RESET_WORD] |= H9_APRS_ADC_CLOCK_BIT;
    board->rcc[H9_APRS_RCC_ADC_RESET_WORD] &= ~H9_APRS_ADC_CLOCK_BIT;
    board->rcc[H9_APRS_RCC_ADC_ENABLE_WORD] |= H9_APRS_ADC_CLOCK_BIT;
    return true;
}

bool h9_aprs_adc_board_calibrate(H9AprsAdcBoard *board,
                                 uint32_t poll_limit,
                                 void (*poll_step)(void *context),
                                 void *context)
{
    uint32_t previous_cfgr;
    uint32_t poll;
    if (board == NULL || board->adc == NULL || poll_limit == 0u ||
        (board->adc[H9_APRS_ADC_CR_WORD] & 1u) != 0u) return false;
    previous_cfgr = board->adc[H9_APRS_ADC_CFGR1_WORD];
    board->adc[H9_APRS_ADC_CFGR1_WORD] = previous_cfgr & ~3u;
    board->adc[H9_APRS_ADC_CR_WORD] |= 0x80000000u;
    for (poll = 0; poll < poll_limit; ++poll) {
        if ((board->adc[H9_APRS_ADC_CR_WORD] & 0x80000000u) == 0u) {
            board->adc[H9_APRS_ADC_CFGR1_WORD] =
                (board->adc[H9_APRS_ADC_CFGR1_WORD] & ~3u) |
                (previous_cfgr & 3u);
            return true;
        }
        if (poll_step != NULL) poll_step(context);
    }
    board->adc[H9_APRS_ADC_CFGR1_WORD] =
        (board->adc[H9_APRS_ADC_CFGR1_WORD] & ~3u) |
        (previous_cfgr & 3u);
    return false;
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

bool h9_aprs_adc_board_configure(H9AprsAdcBoard *board)
{
    volatile uint32_t *adc;
    if (board == NULL || board->adc == NULL || board->rcc == NULL ||
        board->gpio_a == NULL ||
        (board->adc[H9_APRS_ADC_CR_WORD] & 4u) != 0u) return false;
    adc = board->adc;
    h9_aprs_adc_board_prepare_pin(board);
    adc[H9_APRS_ADC_CFGR1_WORD] =
        (adc[H9_APRS_ADC_CFGR1_WORD] & ~0x18u) | 8u;
    adc[H9_APRS_ADC_CFGR2_WORD] =
        (adc[H9_APRS_ADC_CFGR2_WORD] & 0x0FFFFFFFu) | 0x10000000u;
    adc[H9_APRS_ADC_CFGR1_WORD] =
        (adc[H9_APRS_ADC_CFGR1_WORD] & 0xFFFEC219u) | 0x5C04u;
    adc[H9_APRS_ADC_SMPR_WORD] =
        (adc[H9_APRS_ADC_SMPR_WORD] & ~7u) | 7u;
    if ((adc[H9_APRS_ADC_CFGR1_WORD] & 0xC33FFFE7u) != 0x5C04u)
        return false;
    adc[H9_APRS_ADC_CHSELR_WORD] |= 2u;
    return true;
}

bool h9_aprs_adc_board_start(H9AprsAdcBoard *board, uint32_t poll_limit,
                             void (*poll_step)(void *context), void *context)
{
    uint32_t poll;
    volatile uint32_t *adc;
    if (board == NULL || board->adc == NULL || board->nvic_iser == NULL ||
        poll_limit == 0u) return false;
    adc = board->adc;
    if ((adc[H9_APRS_ADC_CR_WORD] & 4u) != 0u) return false;
    adc[H9_APRS_ADC_CR_WORD] |= 1u;
    for (poll = 0; poll < poll_limit; ++poll) {
        if ((adc[H9_APRS_ADC_ISR_WORD] & 1u) != 0u) break;
        if (poll_step != NULL) poll_step(context);
    }
    if (poll == poll_limit) return false;
    adc[H9_APRS_ADC_ISR_WORD] = 0x1Cu;
    adc[H9_APRS_ADC_IER_WORD] |= 0x1Cu;
    board->nvic_iser[0] |= 1u << 12;
    adc[H9_APRS_ADC_CR_WORD] |= 4u;
    return true;
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

uint32_t h9_aprs_adc_board_irq(H9AprsAdcBoard *board,
                                H9AprsController *controller)
{
    uint32_t pending;
    uint32_t handled = 0u;
    volatile uint32_t *adc;
    if (board == NULL || board->adc == NULL || controller == NULL) return 0u;
    adc = board->adc;
    pending = adc[H9_APRS_ADC_ISR_WORD] & adc[H9_APRS_ADC_IER_WORD];
    if ((pending & H9_APRS_ADC_SAMPLE_FLAGS) != 0u) {
        if ((board->irq_flags & 0x10u) == 0u)
            board->irq_flags |= 0x200u;
        if ((pending & 8u) != 0u &&
            (adc[H9_APRS_ADC_CFGR1_WORD] & 0xC00u) == 0u &&
            board->state_byte_19 == 0u) {
            if ((adc[H9_APRS_ADC_CR_WORD] & 4u) == 0u) {
                adc[H9_APRS_ADC_IER_WORD] &= ~0xCu;
                board->irq_flags = (board->irq_flags & ~0x100u) | 1u;
            } else {
                board->irq_flags |= 0x20u;
                board->event_flags |= 1u;
            }
        }
        h9_aprs_controller_adc_tick(controller);
        adc[H9_APRS_ADC_ISR_WORD] = H9_APRS_ADC_SAMPLE_FLAGS;
        handled |= pending & H9_APRS_ADC_SAMPLE_FLAGS;
    }
    if ((pending & 0x80u) != 0u) {
        board->irq_flags |= 0x10000u;
        adc[H9_APRS_ADC_ISR_WORD] = 0x80u;
        handled |= 0x80u;
    }
    if ((pending & 0x10u) != 0u) {
        if (board->state_word_28 == 1u ||
            (adc[H9_APRS_ADC_CFGR1_WORD] & 1u) != 0u)
            board->event_flags |= 2u;
        adc[H9_APRS_ADC_ISR_WORD] = 0x10u;
        handled |= 0x10u;
    }
    return handled;
}
