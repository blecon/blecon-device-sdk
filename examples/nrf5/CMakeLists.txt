# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(nrf5-sdk)
target_link_libraries(blecon_nrf5 PRIVATE nrf-sdk-base nrf-sdk-app)

add_executable(app-example app-example.c main.c nrf_log_backend_stdout.c)
target_link_libraries(app-example PRIVATE blecon_nrf5 blecon)
nrf5_app(app-example)