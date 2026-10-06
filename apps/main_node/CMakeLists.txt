cmake_minimum_required(VERSION 3.20.0)

set(ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/../../../../modules/lib/micro_ros/modules/libmicroros CACHE STRING "micro-ROS")
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(deos_firmware)


target_sources(app PRIVATE
    src/main.cpp
    src/MainApp.cpp
    src/io/LedController.cpp
    src/io/SafetyManager.cpp
    src/network/NetworkMonitor.cpp
    src/transport/eth_textual.cpp
    src/transport/eth_uros.cpp
    src/transport/uart_lora.cpp
    ../../lib/deos_core/src/deos_core.c
    ../../lib/deos_core/src/deos_rx.c
    ../../lib/deos_core/src/deos_tx.c
    ../../lib/deos_core/src/deos_dispatch.c
    ../../lib/deos_core/src/deos_codec.c
    ../../lib/deos_core/src/deos_network.c
    ../../lib/deos_core/src/deos_router.c
    ../../lib/deos_core/src/deos_fault.c
)

zephyr_include_directories(../../lib/deos_core/include)
