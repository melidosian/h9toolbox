// SPDX-License-Identifier: GPL-2.0-or-later
// TD-H9 port of DMC100-Doom: platform counters.
//
// The bring-up builds wrapped the engine's start-up stages (GNU ld --wrap)
// to show which stage a hang was in; the release keeps only the counters the
// platform hooks bump, and td_trace_counters does nothing.

#include <stdint.h>

#include "td.h"

volatile uint32_t td_nor_reads, td_pump_strips, td_time_calls, td_tic_calls;

void td_trace_counters(int force)
{
    (void)force;
}
