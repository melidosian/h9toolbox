#ifndef H9_MAIN_RF_OUTPUT_H
#define H9_MAIN_RF_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

/* Register operations observed around stock TX/RX transitions. The physical
 * meaning of each line has not been measured; do not infer PTT from a name. */
typedef enum {
    H9_MAIN_RF_GPIO_BIT0 = 0x001u,
    H9_MAIN_RF_GPIO_BIT8 = 0x100u
} H9MainRfGpioLine;

typedef struct {
    volatile uint32_t *gpio_data;
    volatile uint32_t *sync_register;
    volatile uint32_t *pwm_registers;
} H9MainRfOutput;

/* Target bindings mirror stock addresses 0x001E5000, 0x001E0200, and
 * 0x001E0600. No output is changed by binding. */
void h9_main_rf_output_bind_target(H9MainRfOutput *output);

/* Ports app+0x050720 (bit 0) and app+0x058E58 (bit 8). The sync register is
 * read before the GPIO read-modify-write as in stock. */
bool h9_main_rf_output_set_gpio(H9MainRfOutput *output,
                                H9MainRfGpioLine line, bool enabled);

/* Ports app+0x058E80: registers +8/+12 hold period and duty, and bit 8 of
 * register +0 enables the output. duty is 0..255. */
bool h9_main_rf_output_set_pwm(H9MainRfOutput *output, bool enabled,
                               uint8_t duty);

#endif
