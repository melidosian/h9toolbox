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
    H9MainGpsSentenceType type =
        h9_main_gps_parse_fix(sentence, length, &session->gps_fix);
    if (type != H9_MAIN_GPS_UNRECOGNIZED &&
        session->callbacks.gps_fix != NULL)
        session->callbacks.gps_fix(session->callback_context, type,
                                   &session->gps_fix);
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

/* The stock read routine checks this six-byte control header before copying
 * ordinary serial data into the APRS ring (app+0x078F2C..0x078F6E). */
static bool consume_py_control(H9MainAprsSession *session,
                               const uint8_t *bytes, size_t length)
{
    static const uint8_t thd_reply[] = {
        'T', 'H', 'D', 0u, 1u, 19u, 20u
    };
    uint8_t checksum;
    if (length < 6u || bytes[0] != 'P' || bytes[1] != 'Y' ||
        bytes[2] != 2u || bytes[3] != 1u) return false;
    checksum = (uint8_t)(bytes[0] + bytes[1] + bytes[2] + bytes[3] + bytes[4]);
    if (bytes[5] != checksum) return false;
    if (bytes[4] == 0x55u) {
        session->py_ready = true;
    } else if (bytes[4] == 0x0Du &&
               session->callbacks.serial_write != NULL) {
        session->callbacks.serial_write(session->callback_context,
                                        thd_reply, sizeof thd_reply);
    }
    return true;
}

size_t h9_main_aprs_session_poll(H9MainAprsSession *session)
{
    size_t received;
    if (session == NULL) return 0u;
    if (session->callbacks.serial_read != NULL) {
        received = session->callbacks.serial_read(session->callback_context,
                                                  session->serial_read_buffer,
                                                  sizeof session->serial_read_buffer);
        if (received > sizeof session->serial_read_buffer) {
            session->serial_read_faults++;
        } else if (!consume_py_control(session, session->serial_read_buffer,
                                       received)) {
            (void)h9_main_uart_ring_push_batch(&session->uart_ring,
                                                session->serial_read_buffer,
                                                received);
        }
    }
    return h9_main_uart_ring_drain(&session->uart_ring, &session->receiver,
                                    dispatch_frame, session);
}

bool h9_main_aprs_session_take_ready(H9MainAprsSession *session)
{
    bool ready;
    if (session == NULL) return false;
    ready = session->py_ready;
    session->py_ready = false;
    return ready;
}
