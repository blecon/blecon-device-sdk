# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

add_library(blecon_nrf5 STATIC)

target_sources(blecon_nrf5 PRIVATE
  src/blecon_nrf5_bluetooth.c
  src/blecon_nrf5_crypto.c
  src/blecon_nrf5_event_loop.c
  src/blecon_nrf5_gatts_bearer.c
  src/blecon_nrf5_nfc.c
  src/blecon_nrf5_nvm.c
  src/blecon_nrf5_l2cap_server.c
  src/blecon_nrf5_aead_cipher.h
)
target_sources(blecon_nrf5 PUBLIC
  src/blecon_nrf5_mutex.c
)
target_include_directories(blecon_nrf5 PRIVATE src)
target_include_directories(blecon_nrf5 PUBLIC include)
target_include_directories(blecon_nrf5 PRIVATE include/blecon_nrf5)

target_link_libraries(blecon_nrf5 PRIVATE blecon)

