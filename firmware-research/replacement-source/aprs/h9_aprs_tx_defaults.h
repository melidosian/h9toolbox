#ifndef H9_APRS_TX_DEFAULTS_H
#define H9_APRS_TX_DEFAULTS_H

/* APRS v1.0.15 startup data at RAM 0x20000110 and 0x20000114.
 * FUN_08002664 converts each millisecond setting to 1200-baud flag count. */
enum {
    H9_APRS_STOCK_TX_LEAD_MS = 500,
    H9_APRS_STOCK_TX_TAIL_MS = 100
};

#endif
