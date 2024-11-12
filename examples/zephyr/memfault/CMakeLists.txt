# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

include(../common/config.cmake)
include(../common/flash_debug.cmake)

set(EXTRA_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/memfault.conf)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

set(CMAKE_C_STANDARD 11)

project(blecon-memfault)
target_sources(app PRIVATE main.c ota.c)

zephyr_include_directories(config)
