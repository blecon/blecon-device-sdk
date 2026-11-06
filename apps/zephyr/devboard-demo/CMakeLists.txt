# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

include(../common/config.cmake)
include(../common/flash_debug.cmake)

set(BLECON_DEFAULT_LOGGING_LEVEL 0 CACHE STRING "Default logging level" FORCE)

set(EXTRA_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/devboard-demo.conf)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(blecon-devboard-demo)
target_sources(app PRIVATE main.c blecon_led.c)
