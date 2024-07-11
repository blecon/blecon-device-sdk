# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

# For now most parameters are hardcoded - this can change in the future

if(BLECON_NRF5_TARGET STREQUAL "nrf52810_xxaa")
set(CPU CORTEX_M4)
elseif(BLECON_NRF5_TARGET STREQUAL "nrf52832_xxaa")
set(CPU CORTEX_M4F)
elseif(BLECON_NRF5_TARGET STREQUAL "nrf52840_xxaa")
set(CPU CORTEX_M4F)
elseif(BLECON_NRF5_TARGET STREQUAL "nrf52833_xxaa")
set(CPU CORTEX_M4F)
else()
message(FATAL_ERROR "Unknown target ${BLECON_NRF5_TARGET}")
endif()

set(FLAGS_ARCH_CORTEX_M4 "-mcpu=cortex-m4 -mthumb -mabi=aapcs -mfloat-abi=soft")
set(FLAGS_ARCH_CORTEX_M4F "-mcpu=cortex-m4 -mthumb -mabi=aapcs -mfloat-abi=hard -mfpu=fpv4-sp-d16")

set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_SYSTEM_PROCESSOR "ARM")
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

set(CMAKE_ASM_COMPILER "arm-none-eabi-gcc")
set(CMAKE_C_COMPILER "arm-none-eabi-gcc")
set(CMAKE_CXX_COMPILER "arm-none-eabi-g++")

set(CFLAGS_COMMON "-ffunction-sections -fdata-sections -fno-strict-aliasing -fno-builtin --short-enums -Wall -Wno-attributes -Wno-format")
set(ASMFLAGS_COMMON "")

# Do not use --specs=nano.specs as we need full printf support
set(LDFLAGS_COMMON "-Wl,--gc-sections -lc -lnosys -lm")
set(FLAGS_ARCH "${FLAGS_ARCH_${CPU}}")

set(CMAKE_C_FLAGS_INIT "${FLAGS_ARCH} ${CFLAGS_COMMON}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_INIT "${FLAGS_ARCH} ${CFLAGS_COMMON}" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS_INIT "${FLAGS_ARCH} ${ASMFLAGS_COMMON}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_INIT "${FLAGS_ARCH} ${LDFLAGS_COMMON}" CACHE STRING "" FORCE)

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASE "-Os" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-Os -g3" CACHE STRING "" FORCE)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-Os" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Os -g3" CACHE STRING "" FORCE)

set(CMAKE_ASM_FLAGS_DEBUG "-O0 -g3" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS_RELEASE "-Os" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS_RELWITHDEBINFO "-Os -g3" CACHE STRING "" FORCE)

set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-O0 -g3" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-Os" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "-Os -g3" CACHE STRING "" FORCE)
