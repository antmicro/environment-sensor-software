# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app LANGUAGES CXX VERSION 1.0.0)

target_include_directories(app PRIVATE ${CMAKE_BINARY_DIR}/app/include src)

target_sources(app PRIVATE src/main.cpp)
target_sources(app PRIVATE src/fram.cpp)
target_sources(app PRIVATE src/sensors.cpp)
target_sources(app PRIVATE src/rtc.cpp)
target_sources(app PRIVATE src/user_config.cpp)
