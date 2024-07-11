# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

cmake_policy(SET CMP0152 NEW)
include(ExternalProject)
include(FetchContent)
get_filename_component(GNU_INSTALL_ROOT ${CMAKE_C_COMPILER} DIRECTORY)

add_library(nrf-sdk-base INTERFACE)
add_library(nrf-sdk-app INTERFACE)

set(BLECON_NRF5_SDK_LOCATION "" CACHE STRING "Nordic nRF5 SDK location")
if(BLECON_NRF5_SDK_LOCATION STREQUAL "")
set(BLECON_NRF5_SDK_DOWNLOAD_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/nrf5-sdk" CACHE STRING "Nordic nRF5 SDK Download location")

if(BLECON_NRF5_SDK_DOWNLOAD_LOCATION STREQUAL "")
set(DOWNLOAD_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/nrf5-sdk")
else()
file(REAL_PATH ${BLECON_NRF5_SDK_DOWNLOAD_LOCATION} DOWNLOAD_LOCATION BASE_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

FetchContent_Declare(ext-nrf5-sdk EXCLUDE_FROM_ALL
    URL "https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/sdks/nrf5/binaries/nrf5_sdk_17.1.0_ddde560.zip"
    SOURCE_DIR ${DOWNLOAD_LOCATION}
    PATCH_COMMAND dos2unix components/libraries/crypto/backend/oberon/oberon_backend_chacha_poly_aead.c && patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/nrf5-sdk_v17.1.0.patch
    UPDATE_COMMAND "" # Prevents CMake from re-running this step each time a build is invoked
    DOWNLOAD_EXTRACT_TIMESTAMP false
)
FetchContent_MakeAvailable(ext-nrf5-sdk)
set(NORDIC_NRF5_SDK_ROOT "${DOWNLOAD_LOCATION}" CACHE INTERNAL "")
add_dependencies(nrf-sdk-base ext-nrf5-sdk)
else()
set(NORDIC_NRF5_SDK_ROOT "${BLECON_NRF5_SDK_LOCATION}" CACHE INTERNAL "")
endif()

# Use CACHE INTERNAL "" to make these visible in functions below
set(LINKER_SCRIPTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/linker" CACHE INTERNAL "")

if(BLECON_NRF5_TARGET STREQUAL "nrf52810_xxaa")
set(NORDIC_SOFTDEVICE S113 CACHE INTERNAL "")
set(NORDIC_ABI FLOAT_ABI_SOFT)
set(NORDIC_NRFUTIL_FAMILY "NRF52" CACHE INTERNAL "")
elseif(BLECON_NRF5_TARGET STREQUAL "nrf52832_xxaa")
set(NORDIC_SOFTDEVICE S113 CACHE INTERNAL "")
set(NORDIC_ABI FLOAT_ABI_HARD)
set(NORDIC_NRFUTIL_FAMILY "NRF52" CACHE INTERNAL "") # See https://infocenter.nordicsemi.com/index.jsp?topic=%2Fug_nrfutil%2FUG%2Fnrfutil%2Fnrfutil_settings_generate_display.html
elseif(BLECON_NRF5_TARGET STREQUAL "nrf52840_xxaa")
set(NORDIC_SOFTDEVICE S140 CACHE INTERNAL "")
set(NORDIC_ABI FLOAT_ABI_HARD)
set(NORDIC_NRFUTIL_FAMILY "NRF52840" CACHE INTERNAL "")
elseif(BLECON_NRF5_TARGET STREQUAL "nrf52833_xxaa")
set(NORDIC_SOFTDEVICE S113 CACHE INTERNAL "")
set(NORDIC_ABI FLOAT_ABI_HARD)
set(NORDIC_NRFUTIL_FAMILY "NRF52" CACHE INTERNAL "")
else()
message(FATAL_ERROR "Unknown target ${BLECON_NRF5_TARGET}")
endif()

string(TOLOWER ${NORDIC_SOFTDEVICE} NORDIC_SOFTDEVICE_LOWERCASE_)
set(NORDIC_SOFTDEVICE_LOWERCASE "${NORDIC_SOFTDEVICE_LOWERCASE_}" CACHE INTERNAL "")
set(NORDIC_SOFTDEVICE_FILENAME_S113 "s113_nrf52_7.2.0_softdevice.hex" CACHE INTERNAL "")
set(NORDIC_SOFTDEVICE_FILENAME_S140 "s140_nrf52_7.2.0_softdevice.hex" CACHE INTERNAL "")
set(NORDIC_SOFTDEVICE_FILENAME "${NORDIC_SOFTDEVICE_FILENAME_${NORDIC_SOFTDEVICE}}" CACHE INTERNAL "")

# Softdevice IDs are available in the SD release notes
set(NORDIC_SOFTDEVICE_ID_S113 0x102 CACHE INTERNAL "")
set(NORDIC_SOFTDEVICE_ID_S140 0x100 CACHE INTERNAL "")
set(NORDIC_SOFTDEVICE_ID "${NORDIC_SOFTDEVICE_ID_${NORDIC_SOFTDEVICE}}" CACHE INTERNAL "")


target_include_directories(
    nrf-sdk-base INTERFACE
    ${NORDIC_NRF5_SDK_ROOT}/components/softdevice/${NORDIC_SOFTDEVICE_LOWERCASE}/headers/nrf52
    ${NORDIC_NRF5_SDK_ROOT}/components/softdevice/${NORDIC_SOFTDEVICE_LOWERCASE}/headers
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_ancs_c
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_ias_c
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/pwm
    ${NORDIC_NRF5_SDK_ROOT}/external/nrf_oberon/include
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/usbd/class/cdc/acm
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/usbd/class/hid/generic
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/usbd/class/msc
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/usbd/class/hid
    ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/hal
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/log
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_gls
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/fstorage
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/mutex
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/gpiote
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/bootloader/ble_dfu
    ${NORDIC_NRF5_SDK_ROOT}/components/boards
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_advertising
    ${NORDIC_NRF5_SDK_ROOT}/external/utf_converter
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_bas_c
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/micro_ecc
    ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/include
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/experimental_task_manager
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_hrs_c
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/oberon
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/queue
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/pwr_mgmt
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_dtm
    ${NORDIC_NRF5_SDK_ROOT}/components/toolchain/cmsis/include
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_rscs_c
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/common
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_lls
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/bsp
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/nrf_hw
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_bas
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/mpu
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/experimental_section_vars
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_ans_c
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/slip
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/delay
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/usbd/class/cdc
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/memobj
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/cc310_bl
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_nus_c
    ${NORDIC_NRF5_SDK_ROOT}/components/softdevice/common
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_ias
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/usbd/class/hid/mouse
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/low_power_pwm
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_dfu
    ${NORDIC_NRF5_SDK_ROOT}/external/fprintf
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/svc
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/atomic
    ${NORDIC_NRF5_SDK_ROOT}/components
    ${NORDIC_NRF5_SDK_ROOT}/external/nrf_cc310/include
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/scheduler
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/cli
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_lbs
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_hts
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crc16
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/util
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/csense_drv
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/csense
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/balloc
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/ecc
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/hardfault
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_cscs
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/hci
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/usbd/class/hid/kbd
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/timer
    ${NORDIC_NRF5_SDK_ROOT}/integration/nrfx
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/stack_info
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/sortlist
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/spi_mngr
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/led_softblink
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/sdcard
    ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/mdk
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_cts_c
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_nus
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/twi_mngr
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_hids
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/strerror
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crc32
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/optiga
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/usbd/class/audio
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/sensorsim
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/nrf_sw
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/uart
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/fifo
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/peer_manager
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/mem_manager
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/ringbuf
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_tps
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_rscs
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/mbedtls
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/cc310
    ${NORDIC_NRF5_SDK_ROOT}/external/nrf_oberon
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_dis
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/nrf_ble_gatt
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/nrf_ble_qwr
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/gfx
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/button
    ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/twi_sensor
    ${NORDIC_NRF5_SDK_ROOT}/integration/nrfx/legacy
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/usbd
    ${NORDIC_NRF5_SDK_ROOT}/external/mbedtls/include
    ${NORDIC_NRF5_SDK_ROOT}/external/segger_rtt
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/atomic_fifo
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_lbs_c
    ${NORDIC_NRF5_SDK_ROOT}/external/nrf_tls/mbedtls/nrf_crypto/config
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_racp
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/fds
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/atomic_flags
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_hrs
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/cifra
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/stack_guard
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/log/src
    ${NORDIC_NRF5_SDK_ROOT}/components/nfc/t2t_lib
    ${NORDIC_NRF5_SDK_ROOT}/components/nfc/ndef/uri
    ${NORDIC_NRF5_SDK_ROOT}/components/nfc/ndef/generic/message
    ${NORDIC_NRF5_SDK_ROOT}/components/nfc/ndef/generic/record
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/usbd
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/usbd/class/cdc
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/usbd/class/cdc/acm
    ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_dfu
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/bootloader/dfu
    ${NORDIC_NRF5_SDK_ROOT}/components/libraries/bootloader
)

target_include_directories(
    nrf-sdk-app INTERFACE
    config
)
target_link_libraries(nrf-sdk-app INTERFACE
    nrf-sdk-base
)

target_compile_definitions(nrf-sdk-base INTERFACE 
    BLE_STACK_SUPPORT_REQD
    CONFIG_GPIO_AS_PINRESET
    ${NORDIC_ABI}
    MBEDTLS_CONFIG_FILE=\"nrf_crypto_mbedtls_config.h\"
    NRFX_COREDEP_DELAY_US_LOOP_CYCLES=3
    NRF_CRYPTO_MAX_INSTANCE_COUNT=1
    NRF_SD_BLE_API_VERSION=7
    ${NORDIC_SOFTDEVICE}
    SOFTDEVICE_PRESENT
)

if(BLECON_NRF5_TARGET STREQUAL "nrf52810_xxaa")
target_compile_definitions(nrf-sdk-base INTERFACE 
    __STACK_SIZE=2048
)
else()
target_compile_definitions(nrf-sdk-app INTERFACE 
    __STACK_SIZE=4096
)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
target_compile_definitions(nrf-sdk-base INTERFACE 
    DEBUG=1 # Used by NRF SDK to halt on error if in debug mode and a debugger is connected
)
endif()
    
target_compile_definitions(nrf-sdk-app INTERFACE 
    APP_TIMER_V2
    APP_TIMER_V2_RTC1_ENABLED
    BL_SETTINGS_ACCESS_ONLY
)

if(BLECON_NRF5_TARGET STREQUAL "nrf52810_xxaa")
target_compile_definitions(nrf-sdk-app INTERFACE 
    __HEAP_SIZE=4096
)
else()
target_compile_definitions(nrf-sdk-app INTERFACE 
    __HEAP_SIZE=16384
)
endif()

# Board
if(BLECON_NRF5_BOARD STREQUAL "NRF52_DK")
    target_compile_definitions(nrf-sdk-base INTERFACE BOARD_PCA10040)
    set(NORDIC_BOARD_SUPPORTS_NFC 1 CACHE INTERNAL "")
elseif(BLECON_NRF5_BOARD STREQUAL "NRF52840_DK")
    target_compile_definitions(nrf-sdk-base INTERFACE BOARD_PCA10056)
    set(NORDIC_BOARD_SUPPORTS_NFC 1 CACHE INTERNAL "")
elseif(BLECON_NRF5_BOARD STREQUAL "NRF52833_DK")
    target_compile_definitions(nrf-sdk-base INTERFACE BOARD_PCA10100)
    set(NORDIC_BOARD_SUPPORTS_NFC 1 CACHE INTERNAL "")
else()
    message(FATAL_ERROR "Unsupported Nordic board")
endif()

# Target
if(BLECON_NRF5_TARGET STREQUAL "nrf52832_xxaa")
target_compile_definitions(nrf-sdk-base INTERFACE 
    NRF52
    NRF52832_XXAA
    NRF52_PAN_74
)
set(NORDIC_TARGET_SUPPORTS_NFC ${NORDIC_BOARD_SUPPORTS_NFC} CACHE INTERNAL "")
set(NORDIC_TARGET_SUPPORTS_CRYPTOCELL 0 CACHE INTERNAL "")
set(IDENTITY_PAGE_ADDRESS "0x70000")
elseif(BLECON_NRF5_TARGET STREQUAL "nrf52840_xxaa")
target_compile_definitions(nrf-sdk-base INTERFACE 
    NRF52840_XXAA
)
set(NORDIC_TARGET_SUPPORTS_NFC ${NORDIC_BOARD_SUPPORTS_NFC} CACHE INTERNAL "")
set(NORDIC_TARGET_SUPPORTS_CRYPTOCELL 1 CACHE INTERNAL "")
set(IDENTITY_PAGE_ADDRESS "0xF7000")
elseif(BLECON_NRF5_TARGET STREQUAL "nrf52833_xxaa")
target_compile_definitions(nrf-sdk-base INTERFACE 
    NRF52833_XXAA
)
set(NORDIC_TARGET_SUPPORTS_NFC ${NORDIC_BOARD_SUPPORTS_NFC} CACHE INTERNAL "")
set(NORDIC_TARGET_SUPPORTS_CRYPTOCELL 0 CACHE INTERNAL "")
set(IDENTITY_PAGE_ADDRESS "0x77000")
elseif(BLECON_NRF5_TARGET STREQUAL "nrf52810_xxaa")
target_compile_definitions(nrf-sdk-base INTERFACE 
    DEVELOP_IN_NRF52832
    NRF52810_XXAA
    NRF52_PAN_74
)
set(NORDIC_TARGET_SUPPORTS_NFC 0 CACHE INTERNAL "")
set(NORDIC_TARGET_SUPPORTS_CRYPTOCELL 0 CACHE INTERNAL "")
set(IDENTITY_PAGE_ADDRESS "0x2F000")
else()
message(FATAL_ERROR "Unsupported Nordic target")
endif()

if(NORDIC_TARGET_SUPPORTS_NFC)
    target_compile_definitions(nrf-sdk-base INTERFACE NORDIC_TARGET_SUPPORTS_NFC=1)
else()
    target_compile_definitions(nrf-sdk-base INTERFACE NORDIC_TARGET_SUPPORTS_NFC=0)
endif()

if(NORDIC_TARGET_SUPPORTS_CRYPTOCELL)
    target_compile_definitions(nrf-sdk-base INTERFACE NORDIC_TARGET_SUPPORTS_CRYPTOCELL=1)
else()
    target_compile_definitions(nrf-sdk-base INTERFACE NORDIC_TARGET_SUPPORTS_CRYPTOCELL=0)
endif()

add_library(nrf-sdk_oberon STATIC IMPORTED)
add_library(nrf-sdk_nfc_t2t STATIC IMPORTED)
add_library(nrf-sdk_uecc STATIC IMPORTED)

include(ExternalProject)
get_filename_component(GNU_INSTALL_ROOT ${CMAKE_C_COMPILER} DIRECTORY)
ExternalProject_Add(ext-nrf-sdk_uecc EXCLUDE_FROM_ALL
    GIT_REPOSITORY https://github.com/kmackay/micro-ecc.git
    GIT_TAG 24c60e2
    GIT_SHALLOW TRUE
    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/micro-ecc/micro-ecc
    CONFIGURE_COMMAND ""
    INSTALL_COMMAND ""
    UPDATE_COMMAND "" # Prevents CMake from re-runningt this step each time a build is invoked
    BUILD_IN_SOURCE TRUE
    BUILD_COMMAND export GNU_INSTALL_ROOT=${GNU_INSTALL_ROOT}/
        && make PROJ_DIR=${CMAKE_CURRENT_BINARY_DIR}/micro-ecc OUTPUT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/micro-ecc/build -C ${NORDIC_NRF5_SDK_ROOT}/external/micro-ecc/nrf52hf_armgcc/armgcc 
        && make PROJ_DIR=${CMAKE_CURRENT_BINARY_DIR}/micro-ecc OUTPUT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/micro-ecc/build -C ${NORDIC_NRF5_SDK_ROOT}/external/micro-ecc/nrf52nf_armgcc/armgcc
    BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/micro-ecc/nrf52hf_armgcc/armgcc/micro_ecc_lib_nrf52.a
        ${CMAKE_CURRENT_BINARY_DIR}/micro-ecc/nrf52nf_armgcc/armgcc/micro_ecc_lib_nrf52.a
    STEP_TARGETS   build
)

add_dependencies(nrf-sdk_uecc ext-nrf-sdk_uecc-build)
target_include_directories(nrf-sdk_uecc INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/micro-ecc/micro-ecc/)

if(CPU STREQUAL "CORTEX_M4F")
    add_library(nrf-sdk_cc310 STATIC IMPORTED)
    set_target_properties(nrf-sdk_cc310 PROPERTIES IMPORTED_LOCATION ${NORDIC_NRF5_SDK_ROOT}/external/nrf_cc310/lib/cortex-m4/hard-float/libnrf_cc310_0.9.13.a)
    add_library(nrf-sdk_cc310_bl STATIC IMPORTED)
    set_target_properties(nrf-sdk_cc310_bl PROPERTIES IMPORTED_LOCATION ${NORDIC_NRF5_SDK_ROOT}/external/nrf_cc310_bl/lib/cortex-m4/hard-float/libnrf_cc310_bl_0.9.13.a)
    set_target_properties(nrf-sdk_uecc PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/micro-ecc/nrf52hf_armgcc/armgcc/micro_ecc_lib_nrf52.a)
    set_target_properties(nrf-sdk_oberon PROPERTIES IMPORTED_LOCATION ${NORDIC_NRF5_SDK_ROOT}/external/nrf_oberon/lib/cortex-m4/hard-float/liboberon_3.0.8.a)
    set_target_properties(nrf-sdk_nfc_t2t PROPERTIES IMPORTED_LOCATION ${NORDIC_NRF5_SDK_ROOT}/components/nfc/t2t_lib/nfc_t2t_lib_gcc.a)
endif()

if(CPU STREQUAL "CORTEX_M4")
    set_target_properties(nrf-sdk_oberon PROPERTIES IMPORTED_LOCATION ${NORDIC_NRF5_SDK_ROOT}/external/nrf_oberon/lib/cortex-m4/soft-float/liboberon_3.0.8.a)
    set_target_properties(nrf-sdk_uecc PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/micro-ecc/nrf52nf_armgcc/armgcc/micro_ecc_lib_nrf52.a)
    set_target_properties(nrf-sdk_nfc_t2t PROPERTIES IMPORTED_LOCATION ${NORDIC_NRF5_SDK_ROOT}/components/nfc/t2t_lib/nfc_t2t_lib_gcc_no_fpu.a)
endif()

target_link_libraries(nrf-sdk-base INTERFACE
    nrf-sdk_oberon
    nrf-sdk_nfc_t2t
)

if(NORDIC_TARGET_SUPPORTS_CRYPTOCELL)
    target_link_libraries(nrf-sdk-app INTERFACE
        nrf-sdk_cc310
    )
endif()

separate_arguments(NRFJPROG_CMD UNIX_COMMAND PROGRAM SEPARATE_ARGS "nrfjprog $ENV{NRFJPROG_OPTIONS}")
set(NRFJPROG "${NRFJPROG_CMD}" CACHE INTERNAL "")

add_custom_target(flash_softdevice 
    COMMAND echo Flashing: ${NORDIC_SOFTDEVICE_FILENAME}
    COMMAND ${NRFJPROG} -f nrf52 --program \"${NORDIC_NRF5_SDK_ROOT}/components/softdevice/${NORDIC_SOFTDEVICE_LOWERCASE}/hex/${NORDIC_SOFTDEVICE_FILENAME}\" --sectorerase
    COMMAND ${NRFJPROG} -f nrf52 --reset
)

add_custom_target(erase_identity 
COMMAND echo Erasing identity
    COMMAND ${NRFJPROG} -f nrf52 --erasepage ${IDENTITY_PAGE_ADDRESS}
    COMMAND ${NRFJPROG} -f nrf52 --reset
)

add_custom_target(erase_device 
COMMAND echo Erasing device
    COMMAND ${NRFJPROG} -f nrf52 --eraseall
    COMMAND ${NRFJPROG} -f nrf52 --reset
)

# JLink GDB Server
add_custom_target(gdb_server
    COMMAND echo Starting JLink GDB Server
    COMMAND JLinkGDBServer -device ${BLECON_NRF5_TARGET} -if swd -speed auto -ExitOnError
)

# Configure target as NRF5 app
function(nrf5_app TARGET)
    set_target_properties(${TARGET} PROPERTIES SUFFIX ".elf")

    set(LINKER_SCRIPT blecon_${BLECON_NRF5_TARGET}.ld)
    set(TARGET_ELF ${TARGET}.elf)
    set(TARGET_HEX ${TARGET}.hex)
    set(TARGET_FLASH ${TARGET}.flash)
    set(TARGET_GDB ${TARGET}.gdb)
    set(TARGET_MAP ${TARGET}.map)

    # Unfortunately we need to compile SDK sources as part of the app so that NRF sections work
    target_sources(${TARGET} PUBLIC
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_nvmc.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/hal/nrf_nvmc.c 
        ${NORDIC_NRF5_SDK_ROOT}/components/boards/boards.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/nrf_hw/nrf_hw_backend_init.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/nrf_hw/nrf_hw_backend_rng.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/button/app_button.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/scheduler/app_scheduler.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/util/app_util_platform.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crc16/crc16.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/hardfault/hardfault_implementation.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/mem_manager/mem_manager.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/uart/app_uart_fifo.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/uart/retarget.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/fifo/app_fifo.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/util/nrf_assert.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/atomic_fifo/nrf_atfifo.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/atomic/nrf_atomic.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/balloc/nrf_balloc.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/balloc/nrf_balloc.c
        ${NORDIC_NRF5_SDK_ROOT}/external/fprintf/nrf_fprintf.c
        ${NORDIC_NRF5_SDK_ROOT}/external/fprintf/nrf_fprintf_format.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/fstorage/nrf_fstorage.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/fstorage/nrf_fstorage_sd.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/fstorage/nrf_fstorage_nvmc.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/memobj/nrf_memobj.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/pwr_mgmt/nrf_pwr_mgmt.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/queue/nrf_queue.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/ringbuf/nrf_ringbuf.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/experimental_section_vars/nrf_section_iter.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/sortlist/nrf_sortlist.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/strerror/nrf_strerror.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/sensorsim/sensorsim.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/log/src/nrf_log_backend_rtt.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/log/src/nrf_log_backend_serial.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/log/src/nrf_log_backend_uart.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/log/src/nrf_log_default_backends.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/log/src/nrf_log_frontend.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/log/src/nrf_log_str_formatter.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/nrf_crypto_aead.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/nrf_crypto_aes.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/nrf_crypto_aes_shared.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/nrf_crypto_ecc.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/nrf_crypto_ecdh.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/nrf_crypto_ecdsa.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/nrf_crypto_eddsa.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/nrf_crypto_error.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/nrf_crypto_hash.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/nrf_crypto_hkdf.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/nrf_crypto_hmac.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/nrf_crypto_init.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/nrf_crypto_rng.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/nrf_crypto_shared.c
        ${NORDIC_NRF5_SDK_ROOT}/components/softdevice/common/nrf_sdh.c
        ${NORDIC_NRF5_SDK_ROOT}/components/softdevice/common/nrf_sdh_ble.c
        ${NORDIC_NRF5_SDK_ROOT}/components/softdevice/common/nrf_sdh_soc.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/peer_manager/auth_status_tracker.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/common/ble_advdata.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_advertising/ble_advertising.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/common/ble_srv_common.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/peer_manager/gatt_cache_manager.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/peer_manager/gatts_cache_manager.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/peer_manager/id_manager.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/nrf_ble_gatt/nrf_ble_gatt.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/peer_manager/nrf_ble_lesc.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/nrf_ble_qwr/nrf_ble_qwr.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/peer_manager/peer_data_storage.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/peer_manager/peer_database.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/peer_manager/peer_id.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/peer_manager/peer_manager.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/peer_manager/peer_manager_handler.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/peer_manager/pm_buffer.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/peer_manager/security_dispatcher.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/peer_manager/security_manager.c
        ${NORDIC_NRF5_SDK_ROOT}/external/utf_converter/utf.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_bas/ble_bas.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_dis/ble_dis.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/ble_services/ble_hrs/ble_hrs.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/oberon/oberon_backend_chacha_poly_aead.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/oberon/oberon_backend_ecc.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/oberon/oberon_backend_ecdh.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/oberon/oberon_backend_ecdsa.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/oberon/oberon_backend_eddsa.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/oberon/oberon_backend_hash.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crypto/backend/oberon/oberon_backend_hmac.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/crc32/crc32.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu_svci.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/util/app_error.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/util/app_error_weak.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/util/app_error_handler_gcc.c
    )
    
    if(BLECON_NRF5_TARGET STREQUAL "nrf52832_xxaa")
        target_sources(${TARGET} PUBLIC 
            ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/mdk/system_nrf52.c
            ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/mdk/gcc_startup_nrf52.S
        )
    elseif(BLECON_NRF5_TARGET STREQUAL "nrf52810_xxaa")
        target_sources(${TARGET} PUBLIC 
            ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/mdk/system_nrf52810.c
            ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/mdk/gcc_startup_nrf52810.S
        )
    elseif(BLECON_NRF5_TARGET STREQUAL "nrf52840_xxaa")
        target_sources(${TARGET} PUBLIC 
            ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/mdk/system_nrf52840.c
            ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/mdk/gcc_startup_nrf52840.S
        )
    elseif(BLECON_NRF5_TARGET STREQUAL "nrf52833_xxaa")
        target_sources(${TARGET} PUBLIC 
            ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/mdk/system_nrf52833.c
            ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/mdk/gcc_startup_nrf52833.S
        )
    else()
        message(FATAL_ERROR "Unknown target ${BLECON_NRF5_TARGET}")
    endif()
    
    target_sources(${TARGET} PUBLIC
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/bsp/bsp.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/timer/drv_rtc.c
        ${NORDIC_NRF5_SDK_ROOT}/integration/nrfx/legacy/nrf_drv_clock.c
        ${NORDIC_NRF5_SDK_ROOT}/integration/nrfx/legacy/nrf_drv_power.c
        ${NORDIC_NRF5_SDK_ROOT}/integration/nrfx/legacy/nrf_drv_rng.c
        ${NORDIC_NRF5_SDK_ROOT}/integration/nrfx/legacy/nrf_drv_uart.c
        ${NORDIC_NRF5_SDK_ROOT}/integration/nrfx/legacy/nrf_drv_twi.c
        ${NORDIC_NRF5_SDK_ROOT}/integration/nrfx/legacy/nrf_drv_ppi.c
        ${NORDIC_NRF5_SDK_ROOT}/integration/nrfx/legacy/nrf_drv_spis.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/soc/nrfx_atomic.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_clock.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_gpiote.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/prs/nrfx_prs.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_power.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_rng.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_uart.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_uarte.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_nfct.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_pwm.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_twim.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_saadc.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_spis.c
        ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_ppi.c
        ${NORDIC_NRF5_SDK_ROOT}/external/segger_rtt/SEGGER_RTT.c
        ${NORDIC_NRF5_SDK_ROOT}/external/segger_rtt/SEGGER_RTT_Syscalls_GCC.c
        ${NORDIC_NRF5_SDK_ROOT}/external/segger_rtt/SEGGER_RTT_printf.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/timer/app_timer2.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/fds/fds.c
        ${NORDIC_NRF5_SDK_ROOT}/components/libraries/atomic_flags/nrf_atflags.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/common/ble_conn_params.c
        ${NORDIC_NRF5_SDK_ROOT}/components/ble/common/ble_conn_state.c
    )
    
    if(NORDIC_TARGET_SUPPORTS_NFC)
        target_sources(${TARGET} PUBLIC
            ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_nfct.c
            ${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/drivers/src/nrfx_timer.c
            ${NORDIC_NRF5_SDK_ROOT}/components/nfc/platform/nfc_platform.c
            ${NORDIC_NRF5_SDK_ROOT}/components/nfc/ndef/uri/nfc_uri_msg.c
            ${NORDIC_NRF5_SDK_ROOT}/components/nfc/ndef/uri/nfc_uri_rec.c
            ${NORDIC_NRF5_SDK_ROOT}/components/nfc/ndef/generic/message/nfc_ndef_msg.c
            ${NORDIC_NRF5_SDK_ROOT}/components/nfc/ndef/generic/record/nfc_ndef_record.c
        )
    endif()

    target_link_libraries(${TARGET} PUBLIC nrf-sdk-app)

    set(LINK_FLAGS
        "-L${NORDIC_NRF5_SDK_ROOT}/modules/nrfx/mdk -T\"${LINKER_SCRIPTS_DIR}/${LINKER_SCRIPT}\" -Xlinker -Map=\"${CMAKE_CURRENT_BINARY_DIR}/${TARGET_MAP}\" -Wl,--gc-sections"
    )
    
    # Set linker flags
    set_target_properties(${TARGET} PROPERTIES LINK_FLAGS ${LINK_FLAGS})

    # Create HEX
    add_custom_command(TARGET ${TARGET}
        POST_BUILD
        COMMAND arm-none-eabi-objcopy -O ihex "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_ELF}" "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_HEX}"
        BYPRODUCTS ${TARGET_HEX}
    )

    # Print size
    add_custom_command(
        TARGET ${TARGET}
        POST_BUILD
        COMMAND arm-none-eabi-size "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_ELF}"
    )

    # Flash app
    add_custom_target(${TARGET_FLASH} 
        COMMAND echo Flashing ${TARGET_HEX}
        COMMAND ${NRFJPROG} -f nrf52 --halt
        COMMAND ${NRFJPROG} -f nrf52 --program "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_HEX}" --sectorerase
        COMMAND ${NRFJPROG} -f nrf52 --reset
        DEPENDS ${TARGET}
        VERBATIM
    )

    # GDB
    add_custom_target(${TARGET_GDB} 
        COMMAND echo Starting GDB: ${TARGET_ELF}
        COMMAND arm-none-eabi-gdb -ex "target remote :2331" "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_ELF}"
        DEPENDS ${TARGET}
        USES_TERMINAL
    )
endfunction(nrf5_app)