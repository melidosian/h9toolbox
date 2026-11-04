#include "h9_main_aprs_session.h"

#include <string.h>

void h9_main_aprs_session_init(H9MainAprsSession *session,
                               const H9MainAprsSessionCallbacks *callbacks,
                               void *context)
{
    if (session == NULL) return;
    memset(session, 0, sizeof *session);
    if (callbacks != NULL) session->callbacks = *callbacks;
    session->callback_context = context;
    h9_main_uart_ring_init(&session->uart_ring);
    h9_main_wire_reset(&session->receiver);
    h9_main_aprs_versions_init(&session->versions);
    h9_main_gps_init(&session->gps);
}

static void gps_sentence(void *context, const uint8_t *sentence, size_t length)
{
    H9MainAprsSession *session = (H9MainAprsSession *)context;
    if (session->callbacks.gps_sentence != NULL)
        session->callbacks.gps_sentence(session->callback_context,
                                        sentence, length);
}

static void dispatch_frame(void *context, const H9MainWireReceiver *receiver)
{
    H9MainAprsSession *session = (H9MainAprsSession *)context;
    H9MainControlAction action;
    uint8_t reply[16];
    size_t reply_length;
    switch (receiver->command) {
    case 0x00u:
        if (h9_main_aprs_packet_from_frame(receiver, &session->decoded) &&
            session->callbacks.packet != NULL)
            session->callbacks.packet(session->callback_context,
                                      &session->decoded);
        break;
    case 0x10u:
        (void)h9_main_aprs_gps_from_frame(&session->gps, receiver,
                                           gps_sentence, session);
        break;
    case 0x11u:
        reply_length = h9_main_aprs_gpio_reply_from_frame(receiver, reply,
                                                           sizeof reply);
        if (reply_length != 0u && session->callbacks.serial_write != NULL)
            session->callbacks.serial_write(session->callback_context,
                                             reply, reply_length);
        break;
    case 0x21u:
        if (h9_main_aprs_version_from_frame(receiver, &session->versions) &&
            session->callbacks.version != NULL)
            session->callbacks.version(session->callback_context,
                                        &session->versions);
        break;
    case 0x51u:
        if (h9_main_control_from_frame(receiver, &action) &&
            session->callbacks.control != NULL)
            session->callbacks.control(session->callback_context, action);
        break;
    default:
        break;
    }
}

size_t h9_main_aprs_session_poll(H9MainAprsSession *session)
{
    if (session == NULL) return 0u;
    return h9_main_uart_ring_drain(&session->uart_ring, &session->receiver,
                                    dispatch_frame, session);
}
