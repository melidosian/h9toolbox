#ifndef H9_MAIN_EXCEPTION_H
#define H9_MAIN_EXCEPTION_H

#include <stdint.h>

/* The stock RAM exception entry saves 16 general registers and eight
 * processor words, then calls a large flash-resident crash printer. This
 * diagnostic replacement retains the saved frame in RAM before halting. */
enum { H9_MAIN_EXCEPTION_FRAME_WORDS = 24 };
extern volatile uint32_t h9_main_last_exception[25];

void h9_main_exception_capture(const uint32_t *frame);
void h9_main_default_irq(void);

#endif
