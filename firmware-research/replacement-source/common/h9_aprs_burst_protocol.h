#ifndef H9_APRS_BURST_PROTOCOL_H
#define H9_APRS_BURST_PROTOCOL_H

/* Replacement-to-replacement extension. Stock firmware does not send or
 * handle this command. A two-byte payload is {version, activity flag}. */
#define H9_APRS_BURST_COMMAND 0x52u
#define H9_APRS_BURST_VERSION 1u
#define H9_APRS_BURST_ACTIVE 1u
#define H9_APRS_BURST_RENEW_MS 250u
#define H9_APRS_BURST_LEASE_MS 750u

#endif
