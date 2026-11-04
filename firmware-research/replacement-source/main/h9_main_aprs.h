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
    /* Stock auto modes temporarily change the side bit and restore it. */
    bool restore_side_after_send;
} H9TxPlan;

/* Stock app+0x05A538..0x05A676 updates bit 7 of RAM 0x00011B50 when
   the working frequency matches A or B. The caller must supply frequencies
   in the same units and the condition represented by RAM 0x00011B20. */
uint8_t h9_main_track_side_bit(uint8_t side_state, bool compare_working,
                                uint32_t working_frequency,
                                uint32_t a_frequency, uint32_t b_frequency);
H9Side h9_main_side_from_state(uint8_t side_state);

/* Stock RF-side setup at app+0x058D4C reads 0x11AEC when Dual Watch is
 * enabled; otherwise it reads bit 7 of 0x11B50. A zero route byte selects A,
 * and any nonzero route byte selects B. This identifies setup selection,
 * not measured RF output or the red display line. */
H9Side h9_main_rf_setup_side(bool dual_watch, uint8_t route_side_byte,
                             uint8_t channel_side_state);

/* The A/B setup routines copy these two 10 Hz frequency words from RAM
 * 0x11CA0/0x11CA4 or 0x11CA8/0x11CAC to the working 0x11C8C/0x11C9C.
 * The second word is the normal TX candidate; 0x12BBA selects the first
 * instead. Other channel settings copied by stock remain unported. */
typedef struct {
    uint32_t receive_10hz;
    uint32_t transmit_10hz;
} H9MainRfChannelFrequencies;

bool h9_main_rf_select_working_frequencies(
    bool dual_watch, uint8_t route_side_byte, uint8_t channel_side_state,
    const H9MainRfChannelFrequencies *a,
    const H9MainRfChannelFrequencies *b,
    H9MainRfChannelFrequencies *working, H9Side *selected_side);
bool h9_main_rf_tx_candidate_10hz(
    const H9MainRfChannelFrequencies *working,
    bool use_receive_frequency, uint32_t *candidate_10hz);

/* Policy only: actual audio gates and decoder tap have not been located. */
H9AudioPlan h9_main_audio_plan(const H9MainAprsConfig *config,
                               H9Side received_side);

/* Local comparison at main app+0x086166..0x08619C. RX_OFF bypasses only this
   comparison; other conditions can still reject or disable packet handling. */
bool h9_main_rx_channel_matches(H9RxChoice choice, uint32_t working_rx_hz,
                                uint32_t a_rx_hz, uint32_t b_rx_hz);

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
