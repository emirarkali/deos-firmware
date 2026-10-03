cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(dummy_uart)

target_sources(app PRIVATE src/main.c)

# We need the DEOS Core library to use deos_message_t
target_include_directories(app PRIVATE ../../lib/deos_core/include)
