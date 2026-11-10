/* The APRS core without receive parsing, for ".APRS SEND": packets heard
 * meanwhile stay in the modem ring (2 KiB) for the next screen to read. */
#define APRS_NO_RX
#include "aprs_core.c"
