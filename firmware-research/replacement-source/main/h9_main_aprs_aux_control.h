#ifndef H9_MAIN_APRS_AUX_CONTROL_H
#define H9_MAIN_APRS_AUX_CONTROL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Stock app+0x072F8A sends command 0x11 with payload 1 or 2 only when
 * the APRS serial handle at RAM 0x11CFC is open. The remote GPIOB5 gates
 * raw USART2 traffic. Its external circuit has not been identified. */
typedef bool (*H9MainAprsAuxWrite)(void *context, const uint8_t *frame,
                                    size_t length);

bool h9_main_aprs_aux_set_enabled(bool enabled, bool serial_open,
                                   H9MainAprsAuxWrite write,
                                   void *context);

#endif
