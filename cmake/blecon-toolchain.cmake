# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

include(CMakeDependentOption)

option(BLECON_NRF5_PORT "Compile nRF5 port" OFF)
cmake_dependent_option(BLECON_NRF5_EXAMPLES "nRF5 examples" ON "BLECON_NRF5_PORT" OFF)

# Only include toolchain file if building examples
cmake_dependent_option(BLECON_NRF5_TOOLCHAIN "nRF5 toolchain" ON "BLECON_NRF5_EXAMPLES" OFF)

if(BLECON_NRF5_PORT)
    set(BLECON_NRF5_TARGET nrf52840_xxaa CACHE STRING "NRF5 target")

    if(BLECON_NRF5_EXAMPLES)
        set(BLECON_NRF5_BOARD "NRF52840_DK" CACHE STRING "Board to use")
        if(BLECON_NRF5_BOARD STREQUAL "NRF52_DK")
            set(BLECON_NRF5_TARGET nrf52832_xxaa CACHE INTERNAL "")
        elseif(BLECON_NRF5_BOARD STREQUAL "NRF52840_DK")
            set(BLECON_NRF5_TARGET nrf52840_xxaa CACHE INTERNAL "")
        elseif(BLECON_NRF5_BOARD STREQUAL "NRF52833_DK")
            set(BLECON_NRF5_TARGET nrf52833_xxaa CACHE INTERNAL "")
        else()
            message(FATAL_ERROR "Please select a board to use with BLECON_NRF5_BOARD")
        endif()
        
        if(BLECON_NRF5_TOOLCHAIN)
            include("ports/nrf5/nrf-toolchain.cmake")
        endif()
    endif()
endif()
