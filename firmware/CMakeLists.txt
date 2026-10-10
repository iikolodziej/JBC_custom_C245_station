cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(jbc_station C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(jbc_station
    src/main.c
    src/max6675.c
    src/tempsense.c
    src/pid.c
    src/nextion.c
    src/config.c
    src/channel.c
    src/buttons.c
    src/ui.c
)

target_compile_options(jbc_station PRIVATE -Wall -Wextra -Wno-unused-parameter)

# Liczba kolb: domyslnie z src/config.h, albo:  cmake .. -DNUM_CHANNELS=2
if(DEFINED NUM_CHANNELS)
    target_compile_definitions(jbc_station PRIVATE NUM_CHANNELS=${NUM_CHANNELS})
endif()

target_link_libraries(jbc_station
    pico_stdlib
    hardware_pwm
    hardware_adc
    hardware_gpio
    hardware_uart
    hardware_i2c
    hardware_timer
    hardware_flash
    hardware_watchdog
    hardware_clocks
)

pico_enable_stdio_usb(jbc_station 1)
pico_enable_stdio_uart(jbc_station 0)

pico_add_extra_outputs(jbc_station)
