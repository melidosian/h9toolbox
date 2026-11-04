#ifndef H9_APRS_TX_DEFAULTS_H
#define H9_APRS_TX_DEFAULTS_H

/* APRS v1.0.15 startup data at RAM 0x20000110..0x2000011C.
 * FUN_08002664 converts lead/tail milliseconds to 1200-baud flag counts. */
enum {
    H9_APRS_STOCK_TX_LEAD_MS = 500,
    H9_APRS_STOCK_TX_TAIL_MS = 100,
    H9_APRS_STOCK_TX_PARAMETER3_MS = 200,
    H9_APRS_STOCK_TX_PERSISTENCE = 63
};

#endif
