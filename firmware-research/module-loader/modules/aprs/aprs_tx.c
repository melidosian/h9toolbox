/* Outgoing APRS packets: AX.25 UI frame plus KISS framing, the format stock's
 * KISS transmit routine (0x01E5DBCC) takes. */
#include "aprs_tx.h"

/* One 7-byte AX.25 address field; returns 0 for an invalid callsign. */
static int address(uint8_t *out, const char *call, uint8_t flags)
{
    unsigned n = 0, ssid = 0;
    for (; call[n] && call[n] != '-'; n++) {
        char c = call[n];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (n >= 6 || !((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return 0;
        out[n] = (uint8_t)(c << 1);
    }
    if (!n) return 0;
    for (unsigned k = n; k < 6; k++) out[k] = ' ' << 1;
    if (call[n] == '-') {
        const char *s = call + n + 1;
        if (s[0] < '0' || s[0] > '9') return 0;
        ssid = (unsigned)(s[0] - '0');
        if (s[1]) {
            if (s[1] < '0' || s[1] > '9' || s[2] || ssid != 1) return 0;
            ssid = 10 + (unsigned)(s[1] - '0');
            if (ssid > 15) return 0;
        }
    }
    out[6] = (uint8_t)(0x60 | ssid << 1 | flags);
    return 1;
}

unsigned aprs_tx_kiss(uint8_t *out, const char *src, const char *dest, const char *digi, const char *info)
{
    uint8_t frame[7 * 3 + 2 + APRS_TX_INFO_MAX];
    unsigned n;
    if (!address(frame, dest, 0x80)) return 0;              /* command bit on dest */
    if (!address(frame + 7, src, digi ? 0 : 1)) return 0;
    n = 14;
    if (digi) {
        if (!address(frame + 14, digi, 1)) return 0;        /* last address */
        n = 21;
    }
    frame[n++] = 0x03;                                       /* UI */
    frame[n++] = 0xF0;                                       /* no layer 3 */
    unsigned i = 0;
    for (; info[i]; i++) {
        if (i >= APRS_TX_INFO_MAX || info[i] < ' ' || info[i] > '~') return 0;
        frame[n++] = (uint8_t)info[i];
    }
    if (!i) return 0;
    unsigned k = 0;
    out[k++] = 0xC0;
    out[k++] = 0x00;                                         /* port 0, data */
    for (i = 0; i < n; i++) {
        if (frame[i] == 0xC0) { out[k++] = 0xDB; out[k++] = 0xDC; }
        else if (frame[i] == 0xDB) { out[k++] = 0xDB; out[k++] = 0xDD; }
        else out[k++] = frame[i];
    }
    out[k++] = 0xC0;
    return k;
}

unsigned aprs_tx_mhz(char *out, uint32_t f)
{
    unsigned n = 0, mhz = f / 100000, rest = f % 100000 / 10, d = 0;
    char t[5];
    do { t[d++] = (char)('0' + mhz % 10); mhz /= 10; } while (mhz && d < 5);
    while (d) out[n++] = t[--d];
    out[n++] = '.';
    for (unsigned p = 1000; p; p /= 10) out[n++] = (char)('0' + rest / p % 10);
    out[n] = 0;
    return n;
}
