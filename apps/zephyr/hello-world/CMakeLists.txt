# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

include(../common/config.cmake)
include(../common/flash_debug.cmake)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(blecon-hello-world)
target_sources(app PRIVATE main.c)
