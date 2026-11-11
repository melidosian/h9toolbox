/* Host check of orbit.c (used by test_satpass.py): orbit_cli <lat µdeg> <lon µdeg> <unix> [hours] [want]
 * prints one line per pass: label aos los tca max_el az_aos az_los. */
#include <stdio.h>
#include <stdlib.h>
#include "orbit.h"
#include "sats.h"

static int32_t bam(long micro) { return (int32_t)(((long long)micro * 4294967296LL) / 360000000LL); }

int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    struct observer o;
    orbit_observer(&o, bam(atol(argv[1])), bam(atol(argv[2])));
    uint32_t now = (uint32_t)strtoul(argv[3], 0, 10);
    unsigned hours = argc > 4 ? (unsigned)atoi(argv[4]) : 36, want = argc > 5 ? (unsigned)atoi(argv[5]) : 5;
    struct pass p[64];
    unsigned n = orbit_passes(SATS, SATS_COUNT, &GMST_REF, &o, now, hours, 0, p, want > 64 ? 64 : want);
    for (unsigned i = 0; i < n; i++)
        printf("%s %u %u %u %u %u %u\n", SATS[p[i].sat].name, p[i].aos, p[i].los, p[i].tca, p[i].max_el, p[i].az_aos, p[i].az_los);
    return 0;
}
