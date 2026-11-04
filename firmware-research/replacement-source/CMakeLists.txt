cmake_minimum_required(VERSION 3.20)
project(h9_replacement_source C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(h9_replacement_core STATIC
    common/h9_aprs_wire.c
    main/h9_main_aprs.c
    main/h9_main_aprs_control.c
    main/h9_main_aprs_metadata.c
    main/h9_main_aprs_receive.c
    main/h9_main_aprs_wire.c
    main/h9_main_ax25.c
    main/h9_main_beacon.c
    main/h9_main_tx_service.c
    main/h9_main_uart_ring.c
    aprs/h9_aprs_bitstream.c
    aprs/h9_aprs_adc_board.c
    aprs/h9_aprs_clock.c
    aprs/h9_aprs_demod.c
    aprs/h9_aprs_frame.c
    aprs/h9_aprs_gpio_board.c
    aprs/h9_aprs_oscillator.c
    aprs/h9_aprs_ring.c
    aprs/h9_aprs_sample_timer.c
    aprs/h9_aprs_tx.c
    aprs/h9_aprs_tx_gate.c
    aprs/h9_aprs_tx_packet.c
    aprs/h9_aprs_tx_service.c
    aprs/h9_aprs_uart1_board.c
    aprs/h9_aprs_uart1_dma.c
    aprs/h9_aprs_uart1_tx.c
    aprs/h9_aprs_uart_receive.c
    aprs/h9_aprs_wave_output.c
    aprs/h9_aprs_controller.c)
target_include_directories(h9_replacement_core PUBLIC common main aprs)
if(MSVC)
    target_compile_options(h9_replacement_core PRIVATE /W4 /WX)
else()
    target_compile_options(h9_replacement_core PRIVATE -Wall -Wextra -Werror)
endif()

enable_testing()
add_executable(h9_replacement_tests tests/test_replacement.c)
target_link_libraries(h9_replacement_tests PRIVATE h9_replacement_core)
if(MSVC)
    target_compile_options(h9_replacement_tests PRIVATE /W4 /WX)
else()
    target_compile_options(h9_replacement_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME h9_replacement_tests COMMAND h9_replacement_tests)
