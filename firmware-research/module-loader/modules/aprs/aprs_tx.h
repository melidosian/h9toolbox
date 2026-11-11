/* Building outgoing APRS packets (linked into ".APRS SEND" and the host test). */
#ifndef APRS_TX_H
#define APRS_TX_H
#include <stdint.h>

/* Stock's KISS transmit routine (keys, sends, waits the airtime, unkeys, with stock's own checks), the
 * working TX frequency in 10 Hz units, and the 2 m band H9 Toolbox transmits on. */
#ifndef STOCK_KISS_TX
#define STOCK_KISS_TX 0x01E5DBCCu
#endif
#ifndef TX_FREQ
#define TX_FREQ (*(const volatile uint32_t *)0x11C9C)
#endif
enum { TX_LOW = 14400000, TX_HIGH = 14800000 };

/* Destination for packets from H9 Toolbox Firmware (APZxxx: experimental). */
#define APRS_TX_DEST "APZH9T"
#define APRS_TX_PATH "WIDE1-1"
enum { APRS_TX_INFO_MAX = 67, APRS_TX_KISS_MAX = 2 * (7 * 3 + 2 + APRS_TX_INFO_MAX) + 3 };

/* AX.25 UI frame src>dest,digi:info (no FCS) as one KISS data frame
 * (C0 00 ... C0, C0/DB escaped) into out. Returns its length, or 0 if a
 * callsign is not 1-6 letters/digits with an optional -0..-15 SSID or the
 * info is empty, longer than APRS_TX_INFO_MAX or not printable ASCII. */
unsigned aprs_tx_kiss(uint8_t *out, const char *src, const char *dest, const char *digi, const char *info);
/* "144.3900" from a frequency in 10 Hz units; returns the length. */
unsigned aprs_tx_mhz(char *out, uint32_t f);
#endif
