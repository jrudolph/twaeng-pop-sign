cmake_minimum_required(VERSION 3.13)

include(/home/johannes/git/self/_2021/pico-sdk/external/pico_sdk_import.cmake)

project(twaeng_pop_sign)

pico_sdk_init()

add_executable(twaeng_pop_sign)

# generate the header file into the source tree as it is included in the RP2040 datasheet
pico_generate_pio_header(twaeng_pop_sign ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

target_sources(twaeng_pop_sign PRIVATE twaeng_pop.c fast_hsv2rgb_32bit.c)

target_link_libraries(twaeng_pop_sign PRIVATE pico_stdlib hardware_pio hardware_pwm)
pico_add_extra_outputs(twaeng_pop_sign)
