    .section .text,"ax",@progbits
    .globl resident_probe
resident_probe:
    rts
    .balign 4
    .long h9_rs_init, h9_rs_poll, h9_rs_suppress, h9_rs_resume
    .long h9_rs_acquire, h9_rs_release
    .long h9_bs_init, h9_bs_arm, h9_bs_stop, h9_bs_feed_byte, h9_bs_poll
    .long h9_bs_submitted, h9_bs_wait_for, h9_bs_status
    .long tsp_load, tsp_save, tsp_check_game_directory
