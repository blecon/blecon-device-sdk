# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

add_library(blecon STATIC IMPORTED GLOBAL)
target_include_directories(blecon INTERFACE include)

if(LINUX)
    # If targetting Linux
    set(BLECON_ARCHITECTURE ${CMAKE_LIBRARY_ARCHITECTURE} CACHE STRING "ABI")
elseif(DEFINED ZEPHYR_BASE)
    # If targetting Zephyr
    if(CONFIG_FPU)
        if(CONFIG_FP_HARDABI)
            set(FLOAT_ABI hard)
        else()
            set(FLOAT_ABI softfp)
        endif()
    else()
        set(FLOAT_ABI soft)
    endif()

    if(CONFIG_CPU_CORTEX_M4 OR CONFIG_CPU_CORTEX_M7)
        set(ARCHITECTURE armv7em)
    elseif(CONFIG_CPU_CORTEX_M33 AND CONFIG_ARMV8_M_MAINLINE)
        set(ARCHITECTURE armv8m-main)
    else()
        message(FATAL_ERROR "Unsupported architecture")
    endif()

    set(BLECON_ARCHITECTURE ${ARCHITECTURE}_${FLOAT_ABI}-zephyr  CACHE STRING "ABI")
elseif(DEFINED BLECON_NRF5_TARGET)
    # If targetting the nRF5 SDK
    if(CPU STREQUAL "CORTEX_M4" OR CPU STREQUAL "CORTEX_M4F")
        set(ARCHITECTURE armv7em)
    else()
        message(FATAL_ERROR "Unsupported architecture")
    endif()

    if(CPU STREQUAL "CORTEX_M4F")
        set(FLOAT_ABI hard)
    else()
        set(FLOAT_ABI soft)
    endif()

    set(BLECON_ARCHITECTURE ${ARCHITECTURE}_${FLOAT_ABI}-gnu  CACHE STRING "ABI")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Supported architectures
# armv7em_soft-zephyr
# armv7em_softfp-zephyr
# armv7em_hard-zephyr
# armv8m-main_soft-zephyr
# armv8m-main_softfp-zephyr
# armv8m-main_hard-zephyr
# armv7em_soft-gnu
# armv7em_softfp-gnu
# aarch64-linux-gnu
# x86_64-linux-gnu
set_target_properties(blecon PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/${BLECON_ARCHITECTURE}/libblecon.a)
