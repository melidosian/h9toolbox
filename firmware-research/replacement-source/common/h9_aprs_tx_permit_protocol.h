#ifndef H9_APRS_TX_PERMIT_PROTOCOL_H
#define H9_APRS_TX_PERMIT_PROTOCOL_H

/* Replacement-only main-to-APRS serial command. Stock APRS firmware treats
 * command 0x53 as setting 0x03, so the main must check module identity first.
 * Payload is {version, enable, token_lo, token_hi}. Token zero is invalid;
 * another enable with the same token renews the lease. */
#define H9_APRS_TX_PERMIT_COMMAND 0x53u
#define H9_APRS_TX_PERMIT_VERSION 1u
#define H9_APRS_TX_PERMIT_ENABLE 1u
#define H9_APRS_TX_PERMIT_LEASE_MS 10000u

/* Replacement APRS-to-main notification after the final DAC sample drains.
 * Payload is {version, token_lo, token_hi}; 0x0F remains queue acceptance. */
#define H9_APRS_TX_DRAINED_COMMAND 0x54u
#define H9_APRS_TX_DRAINED_VERSION 1u

#endif
