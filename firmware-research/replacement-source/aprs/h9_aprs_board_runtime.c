#include <stddef.h>

#include "h9_aprs_clock.h"
#include "h9_aprs_boot_banner.h"
#include "h9_aprs_runtime_state.h"
#include "h9_aprs_runtime_status.h"
#include "h9_aprs_status.h"
#include "h9_aprs_tx_defaults.h"

/* Replacement board entry. It is linked into the diagnostic image, but its
 * peripheral routing has not been verified on the physical APRS board. */
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

static bool board_boot_write(void *context, const uint8_t *data, size_t length)
{
    H9AprsRuntimeState *state = (H9AprsRuntimeState *)context;
    return h9_aprs_uart1_tx_write(&state->uart1_tx, data, length) == length;
}

static bool board_read_status(void *context, uint8_t output[48])
{
    return h9_aprs_runtime_status_read((H9AprsRuntimeState *)context, output);
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
    if (h9_aprs_tx_runtime_submit(&state->tx_runtime, packet, length))
        state->active_packet_token = state->tx_permit.token;
}

static void board_tx_settings_changed(void *context,
                                      const H9AprsTxSettings *settings)
{
    H9AprsRuntimeState *state = (H9AprsRuntimeState *)context;
    h9_aprs_tx_service_set_timing(&state->tx_service,
                                   settings->lead_ms, settings->tail_ms);
}

static void board_tx_permit_changed(void *context, bool enabled,
                                    uint16_t token)
{
    H9AprsRuntimeState *state = (H9AprsRuntimeState *)context;
    uint32_t now = h9_aprs_systick_now(&state->systick);
    if (enabled && state->active_packet_token != 0u &&
        state->active_packet_token != token)
        h9_aprs_tx_runtime_set_ready(&state->tx_runtime, false);
    h9_aprs_tx_permit_set(&state->tx_permit, enabled, token, now);
    h9_aprs_tx_runtime_set_ready(&state->tx_runtime,
        h9_aprs_tx_permit_active(&state->tx_permit, now));
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

__attribute__((noreturn)) void h9_aprs_board_main(void)
{
    uint32_t system_clock_hz;
    H9AprsHardware hardware = { 0 };
    h9_aprs_uart1_tx_bind_target(&runtime.uart1_tx, 240000u);
    h9_aprs_uart2_board_bind_target(&runtime.uart2_board, 240000u);
    h9_aprs_uart2_rx_bind_target(&runtime.uart2_rx);
    h9_aprs_uart2_dma_bind_target(&runtime.uart2_dma,
                                   &runtime.uart2_rx);
    hardware.context = &runtime;
    hardware.uart_write = board_uart_write;
    hardware.uart_ok = board_uart_ok;
    hardware.read_status = board_read_status;
    hardware.set_radio_gpio = board_set_radio_gpio;
    hardware.read_radio_gpio = board_read_radio_gpio;
    hardware.aux_uart_write = board_aux_uart_write;
    hardware.read_dac_port = board_read_wave_port;
    hardware.write_dac_port = board_write_wave_port;
    hardware.read_adc = board_read_adc;
    hardware.transmit_aprs_packet = board_submit_packet;
    hardware.tx_settings_changed = board_tx_settings_changed;
    hardware.tx_permit_changed = board_tx_permit_changed;
    hardware.tx_wave_active = board_tx_wave_active;
    hardware.next_tx_wave_sample = board_next_tx_wave;
    h9_aprs_controller_init(&runtime.controller, &hardware);
    h9_aprs_tx_service_init(&runtime.tx_service, 0,
                            H9_APRS_STOCK_TX_LEAD_MS,
                            H9_APRS_STOCK_TX_TAIL_MS);
    h9_aprs_tx_runtime_bind(&runtime.tx_runtime, &runtime.tx_service);
    runtime.tx_permit.enabled = false;
    h9_aprs_uart2_maintenance_init(&runtime.uart2_maintenance);
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
        !h9_aprs_uart2_dma_start(&runtime.uart2_dma) ||
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
    if (!h9_aprs_boot_banner_send(board_boot_write, &runtime))
        for (;;) { }
    for (;;) {
        uint32_t now;
        H9AprsDemodGateState gate_state;
        bool queue_error;
        (void)h9_aprs_uart_receive_process(&runtime.uart_receive,
            &runtime.controller, h9_aprs_uart1_dma_rearm,
            &runtime.uart1_dma);
        h9_aprs_controller_process_received(&runtime.controller);
        if (h9_aprs_uart2_dma_poll(&runtime.uart2_dma,
                                    &runtime.controller))
            ++runtime.forwarded_uart2_batches;
        now = h9_aprs_systick_now(&runtime.systick);
        (void)h9_aprs_controller_report_burst_activity(&runtime.controller,
                                                         now);
        if (h9_aprs_uart2_maintenance_due(&runtime.uart2_maintenance, now,
                                            runtime.uart2_board.faulted,
                                            !runtime.uart2_rx.ready &&
                                            !runtime.uart2_dma.spill_active)) {
            (void)h9_aprs_uart2_board_recover(&runtime.uart2_board,
                                                &runtime.uart2_dma,
                                                system_clock_hz);
        }
        gate_state = h9_aprs_demod_gate_state(&runtime.controller.demod);
        queue_error = gate_state.queue_overflow !=
                      runtime.gate_overflow_seen;
        runtime.gate_overflow_seen = gate_state.queue_overflow;
        /* Replacement command 0x53 carries a bounded permit. The main
         * board adapter must still prove RF route and keying before sending
         * it; this board entry has no main-side adapter. */
        (void)h9_aprs_tx_runtime_step(&runtime.tx_runtime,
                                      h9_aprs_tx_permit_active(
                                          &runtime.tx_permit, now),
                                      gate_state.preamble_seen,
                                      gate_state.in_frame, queue_error,
                                      runtime.controller.tx_settings.persistence);
        if (h9_aprs_tx_service_take_encoded_event(&runtime.tx_service))
            (void)h9_aprs_controller_report_tx_encoded(&runtime.controller);
        if (h9_aprs_tx_service_take_drained_event(&runtime.tx_service)) {
            (void)h9_aprs_controller_report_tx_drained(&runtime.controller,
                                                         runtime.active_packet_token);
            runtime.active_packet_token = 0u;
        }
    }
}

void h9_aprs_dma1_channel1_handler(void)
{
    uint32_t before = runtime.uart1_dma.completed_batches;
    h9_aprs_dma1_channel1_irq(&runtime.uart1_dma);
    if (runtime.uart1_dma.completed_batches != before)
        runtime.last_uart1_batch_tick = h9_aprs_systick_now(&runtime.systick);
}

void h9_aprs_usart1_handler(void)
{
    uint32_t before = runtime.uart1_dma.completed_batches;
    h9_aprs_uart1_irq(&runtime.uart1_dma);
    if (runtime.uart1_dma.completed_batches != before)
        runtime.last_uart1_batch_tick = h9_aprs_systick_now(&runtime.systick);
}

void h9_aprs_adc1_handler(void)
{
    (void)h9_aprs_adc_board_irq(&runtime.adc_board, &runtime.controller);
}

void h9_aprs_systick_handler(void)
{
    h9_aprs_systick_irq(&runtime.systick);
}

void h9_aprs_usart2_handler(void)
{
    h9_aprs_uart2_dma_usart_irq(&runtime.uart2_dma);
}

void h9_aprs_dma1_channel3_handler(void)
{
    h9_aprs_uart2_dma_channel_irq(&runtime.uart2_dma);
}
