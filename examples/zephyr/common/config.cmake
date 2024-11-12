# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

# Force use of C11 standard
set(CMAKE_C_STANDARD 11)
set_property(GLOBAL PROPERTY CSTD c11)

# Set shared config directory
set(APPLICATION_CONFIG_DIR ${CMAKE_CURRENT_LIST_DIR}/config)

# Set Blecon default logging level (warnings and errors)
set(BLECON_DEFAULT_LOGGING_LEVEL 2 CACHE STRING "Default logging level")
