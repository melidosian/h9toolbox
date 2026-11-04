#ifndef H9_MAIN_CHANNEL_DEFAULTS_H
#define H9_MAIN_CHANNEL_DEFAULTS_H

#include <stdint.h>

/* Stock initializer app+0x048A46 first prepares two 164-byte-spaced records
 * in each RAM block, then loads stored channel configuration. This module
 * covers only the verified default-record prefix. */
enum { H9_MAIN_DEFAULT_TEMPLATE_SIZE = 24,
       H9_MAIN_DEFAULT_RECORD_STRIDE = 0xA4 };

extern const uint8_t
    h9_main_channel_default_template[H9_MAIN_DEFAULT_TEMPLATE_SIZE];

typedef struct {
    volatile uint8_t *state_11ad0;   /* at least stride + 7 bytes */
    volatile uint8_t *primary_143b0; /* at least stride + 0x40 bytes */
    volatile uint8_t *secondary_144f8; /* same size */
} H9MainChannelDefaults;

void h9_main_channel_defaults_bind_target(H9MainChannelDefaults *defaults);
int h9_main_channel_defaults_init_prefix(const H9MainChannelDefaults *defaults);

#endif
