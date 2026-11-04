#ifndef H9_MAIN_APRS_H
#define H9_MAIN_APRS_H

#include <stdbool.h>
#include <stdint.h>

/* Values observed at clone image offsets 0x3116 and 0x3117. */
typedef enum { H9_SIDE_A = 0, H9_SIDE_B = 1 } H9Side;
typedef enum { H9_RX_OFF = 0, H9_RX_A = 1, H9_RX_B = 2 } H9RxChoice;
typedef enum {
    H9_TX_A_LOCK = 0,
    H9_TX_B_LOCK = 1,
    H9_TX_A_AUTO = 2,
    H9_TX_B_AUTO = 3,
    H9_TX_CURRENT = 4
} H9TxChoice;

typedef struct {
    bool aprs_enabled;
    bool mute_aprs_audio;
    H9RxChoice rx_choice;
    H9TxChoice tx_choice;
} H9MainAprsConfig;

typedef struct {
    bool decoder_feed;
    bool speaker_audio;
    bool bluetooth_audio;
} H9AudioPlan;

typedef struct {
    bool allowed;
    H9Side side;
    uint32_t expected_tx_hz;
} H9TxPlan;

/* Policy only: actual audio gates and decoder tap have not been located. */
H9AudioPlan h9_main_audio_plan(const H9MainAprsConfig *config,
                               H9Side received_side);

/* Stock mode numbers, but a replacement invariant: never key on a side
   other than the planned side. Lock modes retain the stock current-side guard. */
H9TxPlan h9_main_tx_plan(const H9MainAprsConfig *config, H9Side current_side,
                         uint32_t a_tx_hz, uint32_t b_tx_hz);

/* The hardware adapter must report the selected side and programmed TX
   frequency after RF setup, before a caller initiates PTT. It cannot by
   itself prove emitted RF frequency; that requires a bench measurement. */
bool h9_main_tx_prepared(const H9TxPlan *plan, H9Side programmed_side,
                         uint32_t programmed_tx_hz);

#endif
