# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(nrf5-sdk)
target_link_libraries(blecon_nrf5 PRIVATE nrf-sdk-base nrf-sdk-app)

add_executable(hello-world hello-world.c main.c)
target_link_libraries(hello-world PRIVATE blecon_nrf5 blecon)
nrf5_app(hello-world)

if(NOT BLECON_NRF5_TARGET STREQUAL "nrf52810_xxaa")
    add_executable(large-request large-request.c main.c)
    target_link_libraries(large-request PRIVATE blecon_nrf5 blecon)
    nrf5_app(large-request)

    add_executable(scanner scanner.c main.c)
    target_link_libraries(scanner PRIVATE blecon_nrf5 blecon)
    nrf5_app(scanner)
endif()

if(BLECON_NRF5_LOG)
    target_sources(hello-world PRIVATE nrf_log_backend_stdout.c)
    if(NOT BLECON_NRF5_TARGET STREQUAL "nrf52810_xxaa")
        target_sources(large-request PRIVATE nrf_log_backend_stdout.c)
        target_sources(scanner PRIVATE nrf_log_backend_stdout.c)
    endif()
endif()

if(BLECON_NRF5_BOARD STREQUAL "nrf52dk")
    # Reduce large request buffer size for NRF52_DK
    target_compile_definitions(large-request PRIVATE -DLARGE_REQUEST_EXAMPLE_BUFFER_SZ=16384)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/additional-examples.cmake")
    include(additional-examples.cmake)
endif()
