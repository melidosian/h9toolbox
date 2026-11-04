cmake_minimum_required(VERSION 3.20)
project(h9_replacement_source C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(h9_replacement_core STATIC
    common/h9_aprs_wire.c
    main/h9_main_aprs.c
    aprs/h9_aprs_bitstream.c
    aprs/h9_aprs_ring.c
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
