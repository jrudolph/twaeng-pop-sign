cmake_minimum_required(VERSION 3.13)

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(twang_pop_sign)

pico_sdk_init()

add_executable(twang_pop_sign)

# generate the header file into the source tree as it is included in the RP2040 datasheet
pico_generate_pio_header(twang_pop_sign ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

target_sources(twang_pop_sign PRIVATE ws2812.c)

target_link_libraries(twang_pop_sign PRIVATE pico_stdlib hardware_pio)
pico_add_extra_outputs(twang_pop_sign)
