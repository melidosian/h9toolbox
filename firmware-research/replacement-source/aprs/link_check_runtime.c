#include <stddef.h>

#include "h9_aprs_clock.h"
#include "h9_aprs_runtime_state.h"

static H9AprsRuntimeState runtime;

static void board_uart_write(void *context, const uint8_t *data, size_t length)
{
    H9AprsRuntimeState *state = (H9AprsRuntimeState *)context;
    (void)h9_aprs_uart1_tx_write(&state->uart1_tx, data, length);
}

static bool board_uart_ok(void *context)
{
    H9AprsRuntimeState *state = (H9AprsRuntimeState *)context;
    return h9_aprs_uart1_tx_ok(&state->uart1_tx);
}

static void board_set_radio_gpio(void *context, bool high)
{
    H9AprsRuntimeState *state = (H9AprsRuntimeState *)context;
    h9_aprs_gpio_board_write(&state->gpio_board, high);
}

static bool board_read_radio_gpio(void *context)
{
    H9AprsRuntimeState *state = (H9AprsRuntimeState *)context;
    return h9_aprs_gpio_board_read(&state->gpio_board);
}

static void board_aux_uart_write(void *context, const uint8_t *data,
                                 size_t length)
{
    H9AprsRuntimeState *state = (H9AprsRuntimeState *)context;
    (void)h9_aprs_uart2_board_write(&state->uart2_board, data, length);
}

static uint8_t board_read_wave_port(void *context)
{
    H9AprsRuntimeState *state = (H9AprsRuntimeState *)context;
    return h9_aprs_wave_output_read(&state->wave_output);
}

static void board_write_wave_port(void *context, uint8_t value)
{
    H9AprsRuntimeState *state = (H9AprsRuntimeState *)context;
    h9_aprs_wave_output_write(&state->wave_output, value);
}

static uint16_t board_read_adc(void *context)
{
    H9AprsRuntimeState *state = (H9AprsRuntimeState *)context;
    return h9_aprs_adc_board_read(&state->adc_board);
}

static void board_submit_packet(void *context, const uint8_t *packet,
                                size_t length)
{
    H9AprsRuntimeState *state = (H9AprsRuntimeState *)context;
    (void)h9_aprs_tx_runtime_submit(&state->tx_runtime, packet, length);
}

static bool board_tx_wave_active(void *context)
{
    H9AprsRuntimeState *state = (H9AprsRuntimeState *)context;
    return h9_aprs_tx_runtime_wave_active(&state->tx_runtime);
}

static bool board_next_tx_wave(void *context, uint8_t *sample)
{
    H9AprsRuntimeState *state = (H9AprsRuntimeState *)context;
    return h9_aprs_tx_runtime_next_sample(&state->tx_runtime, sample);
}

/* Temporary freestanding shims for a whole-archive link. */
void *memcpy(void *destination, const void *source, size_t length)
{
    unsigned char *to = (unsigned char *)destination;
    const unsigned char *from = (const unsigned char *)source;
    size_t index;
    for (index = 0; index < length; ++index) to[index] = from[index];
    return destination;
}

void *memset(void *destination, int value, size_t length)
{
    unsigned char *to = (unsigned char *)destination;
    size_t index;
    for (index = 0; index < length; ++index) to[index] = (unsigned char)value;
    return destination;
}

__attribute__((noreturn)) void h9_aprs_board_main(void)
{
    uint32_t system_clock_hz;
    H9AprsHardware hardware = { 0 };
    h9_aprs_uart1_tx_bind_target(&runtime.uart1_tx, 240000u);
    h9_aprs_uart2_board_bind_target(&runtime.uart2_board, 240000u);
    hardware.context = &runtime;
    hardware.uart_write = board_uart_write;
    hardware.uart_ok = board_uart_ok;
    hardware.set_radio_gpio = board_set_radio_gpio;
    hardware.read_radio_gpio = board_read_radio_gpio;
    hardware.aux_uart_write = board_aux_uart_write;
    hardware.read_dac_port = board_read_wave_port;
    hardware.write_dac_port = board_write_wave_port;
    hardware.read_adc = board_read_adc;
    hardware.transmit_aprs_packet = board_submit_packet;
    hardware.tx_wave_active = board_tx_wave_active;
    hardware.next_tx_wave_sample = board_next_tx_wave;
    h9_aprs_controller_init(&runtime.controller, &hardware);
    h9_aprs_tx_service_init(&runtime.tx_service, 0, 0, 0);
    h9_aprs_tx_runtime_bind(&runtime.tx_runtime, &runtime.tx_service);
    h9_aprs_uart_receive_init(&runtime.uart_receive);
    h9_aprs_uart1_dma_bind_target(&runtime.uart1_dma,
                                  &runtime.uart_receive);
    h9_aprs_uart1_board_bind_target(&runtime.uart1_board);
    h9_aprs_gpio_board_bind_target(&runtime.gpio_board);
    h9_aprs_wave_output_bind_target(&runtime.wave_output);
    h9_aprs_adc_board_bind_target(&runtime.adc_board);
    h9_aprs_sample_timer_bind_target(&runtime.sample_timer);
    h9_aprs_systick_bind_target(&runtime.systick);
    system_clock_hz = h9_aprs_clock_select_internal_target(100000u);
    if (system_clock_hz == 0u ||
        !h9_aprs_systick_start(&runtime.systick, system_clock_hz,
                               H9_APRS_STOCK_TICK_STEP_MS) ||
        !h9_aprs_gpio_board_configure(&runtime.gpio_board) ||
        !h9_aprs_uart2_board_configure(&runtime.uart2_board,
                                       system_clock_hz) ||
        !h9_aprs_uart1_board_configure(&runtime.uart1_board,
                                        system_clock_hz) ||
        !h9_aprs_uart1_dma_start(&runtime.uart1_dma))
        for (;;) { }
    h9_aprs_wave_output_configure(&runtime.wave_output);
    if (!h9_aprs_sample_timer_start(&runtime.sample_timer))
        for (;;) { }
    if (!h9_aprs_adc_board_reset_enable(&runtime.adc_board) ||
        !h9_aprs_adc_board_calibrate(&runtime.adc_board, 100000u, NULL, NULL))
        for (;;) { }
    if (!h9_aprs_adc_board_configure(&runtime.adc_board) ||
        !h9_aprs_adc_board_start(&runtime.adc_board, 100000u, NULL, NULL))
        for (;;) { }
    h9_aprs_uart1_board_enable_irqs(&runtime.uart1_board);
    for (;;) {
        (void)h9_aprs_uart_receive_process(&runtime.uart_receive,
            &runtime.controller, h9_aprs_uart1_dma_rearm,
            &runtime.uart1_dma);
        h9_aprs_controller_process_received(&runtime.controller);
        /* RF readiness and carrier sense remain unmapped. This diagnostic
         * entry accepts packets but never authorizes waveform output. */
        (void)h9_aprs_tx_runtime_step(&runtime.tx_runtime, false,
                                      true, false, false, 0u);
    }
}

void h9_aprs_dma1_channel1_handler(void)
{
    h9_aprs_dma1_channel1_irq(&runtime.uart1_dma);
}

void h9_aprs_usart1_handler(void)
{
    h9_aprs_uart1_irq(&runtime.uart1_dma);
}

void h9_aprs_adc1_handler(void)
{
    (void)h9_aprs_adc_board_sample_irq(&runtime.adc_board,
                                        &runtime.controller);
}

void h9_aprs_systick_handler(void)
{
    h9_aprs_systick_irq(&runtime.systick);
}
