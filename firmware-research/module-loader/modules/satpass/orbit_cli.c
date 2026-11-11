/* Host check of orbit.c (used by test_satpass.py): orbit_cli <lat µdeg> <lon µdeg> <unix> [hours] [want]
 * prints one line per pass: label aos los tca max_el az_aos az_los. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "orbit.h"
#include "host_satdata.h"
#include "doppler.h"

static int32_t bam(long micro) { return (int32_t)(((long long)micro * 4294967296LL) / 360000000LL); }

int main(int argc, char **argv)
{
    if (argc >= 6 && !strcmp(argv[1], "range")) {   /* orbit_cli range <lat> <lon> <sat> <unix>: azimuth, elevation, range km, shift at 437.8 MHz */
        struct observer ob;
        orbit_observer(&ob, bam(atol(argv[2])), bam(atol(argv[3])));
        struct look a, before, after;
        struct satdata data;
        struct satentry entry;
        struct gmst_ref gmst;
        if (!satdata_open(host_satdata_api(getenv("SATDATA") ? getenv("SATDATA") : "build/satdata.bin"), &data)) return 3;
        unsigned id = (unsigned)atoi(argv[4]);
        satdata_get(&data, id, &entry);
        satdata_gmst(&data, &gmst);
        uint32_t t = (uint32_t)strtoul(argv[5], 0, 10);
        orbit_look(&entry.sat, &gmst, &ob, t, &a);
        orbit_look(&entry.sat, &gmst, &ob, t - DOPPLER_HALF, &before);
        orbit_look(&entry.sat, &gmst, &ob, t + DOPPLER_HALF, &after);
        printf("%f %f %f %d\n", a.az * 360.0 / 4294967296.0, a.el * 360.0 / 4294967296.0, a.range / 256.0, doppler_shift(43780000u, after.range - before.range));
        return 0;
    }
    if (argc < 4) return 2;
    struct observer o;
    struct satdata data;
    struct gmst_ref gmst;
    if (!satdata_open(host_satdata_api(getenv("SATDATA") ? getenv("SATDATA") : "build/satdata.bin"), &data)) return 3;
    satdata_gmst(&data, &gmst);
    orbit_observer(&o, bam(atol(argv[1])), bam(atol(argv[2])));
    uint32_t now = (uint32_t)strtoul(argv[3], 0, 10);
    unsigned hours = argc > 4 ? (unsigned)atoi(argv[4]) : 36, want = argc > 5 ? (unsigned)atoi(argv[5]) : 5;
    struct pass p[64];
    unsigned n = orbit_passes(host_get_sat, &data, data.head.count, &gmst, &o, now, hours, 0, p, want > 64 ? 64 : want);
    for (unsigned i = 0; i < n; i++)
        { struct satentry e; satdata_get(&data, p[i].sat, &e); printf("%s %u %u %u %u %u %u\n", e.sat.name, p[i].aos, p[i].los, p[i].tca, p[i].max_el, p[i].az_aos, p[i].az_los); }
    return 0;
}
