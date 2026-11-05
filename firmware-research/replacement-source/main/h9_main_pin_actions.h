#ifndef H9_MAIN_PIN_ACTIONS_H
#define H9_MAIN_PIN_ACTIONS_H

#include <stdint.h>

#include "h9_main_pin_registry.h"

/* Four stock GPIO bank pointers at app+0x000554. Each bank has 0x40 bytes.
 * The early my_app pins use only the ordinary 0..55 branch, excluding the
 * separately handled pin 38. */
typedef struct {
    volatile uint32_t *banks[4];
    volatile uint32_t *sync_1e0200;
} H9MainPinActions;

void h9_main_pin_actions_bind_target(H9MainPinActions *actions);
void h9_main_pin_actions_attach(H9MainPinRegistry *registry,
                                 H9MainPinActions *actions);

#endif
