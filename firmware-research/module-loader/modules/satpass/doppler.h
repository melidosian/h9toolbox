/* Doppler shift from the change of the slant range (km, Q8) over a time span of
 * 2^span_log2 seconds: shift = f * (delta / span) / c. 1 / (256 * 299792.458) =
 * 2^-40 * 14326.7, so one multiply and a shift (no 64-bit division). Frequencies
 * are in 10 Hz units. A receding satellite (positive range change) is heard lower
 * and must be sent to higher.
 *
 * The span is centred on the moment of interest (SPAN_BEFORE and SPAN_AFTER
 * seconds either side): a one-second forward difference had two faults: it is
 * the rate half a second ago (up to 25 Hz off at 2 m on an overhead pass), and
 * the range is only known to 1/256 km, so its noise was about 20 Hz at 145.9 MHz
 * (the number on the radio flickered by 10-20 Hz). Eight seconds divide the noise
 * by eight and the error of the centred difference is far under a hertz. */
#ifndef SATPASS_DOPPLER_H
#define SATPASS_DOPPLER_H

#include <stdint.h>

enum { DOPPLER_HALF = 4, DOPPLER_SPAN_LOG2 = 3 };   /* range at t - 4 s and t + 4 s, 8 s apart */

static inline int32_t doppler_shift(uint32_t f10, int32_t delta_q8)
{
    return (int32_t)(((int64_t)f10 * delta_q8 * 14327) >> (40 + DOPPLER_SPAN_LOG2));
}

#endif
