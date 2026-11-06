# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

# Minimum C standard for the examples; the Zephyr build itself follows CONFIG_STD_C* (C17 by default)
set(CMAKE_C_STANDARD 11)

# Set shared config directory
set(APPLICATION_CONFIG_DIR ${CMAKE_CURRENT_LIST_DIR}/config)

# Set Blecon default logging level (warnings and errors)
set(BLECON_DEFAULT_LOGGING_LEVEL 2 CACHE STRING "Default logging level")
