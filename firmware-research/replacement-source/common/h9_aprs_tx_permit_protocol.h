#ifndef H9_APRS_TX_PERMIT_PROTOCOL_H
#define H9_APRS_TX_PERMIT_PROTOCOL_H

/* Replacement-only main-to-APRS serial command. Stock APRS firmware treats
 * command 0x53 as setting 0x03, so the main must check module identity first.
 * Payload is {version, enable}; another enable renews the lease. */
#define H9_APRS_TX_PERMIT_COMMAND 0x53u
#define H9_APRS_TX_PERMIT_VERSION 1u
#define H9_APRS_TX_PERMIT_ENABLE 1u
#define H9_APRS_TX_PERMIT_LEASE_MS 10000u

#endif
