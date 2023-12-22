# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

if(EXISTS  "${CMAKE_CURRENT_SOURCE_DIR}/blecon_zephyr-extra.cmake")
  set(BLECON_ZEPHYR_HAS_EXTRA TRUE)
else()
  set(BLECON_ZEPHYR_HAS_EXTRA FALSE)
endif()


cmake_dependent_option(BLECON_ZEPHYR_EXTRA "Compile extra Zephyr port" ON "BLECON_ZEPHYR_HAS_EXTRA" OFF)

add_library(blecon_zephyr STATIC)

target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_event_loop.c
  src/blecon_zephyr_bluetooth.c
  src/blecon_zephyr_crypto.c
  src/blecon_zephyr_error.c
  src/blecon_zephyr_nfc.c
  src/blecon_zephyr_nvm.c
  src/blecon_zephyr_l2cap_bearer.c
  src/blecon_zephyr_l2cap_server.c
  src/blecon_zephyr_gatts_bearer.c
  src/blecon_zephyr_aead_cipher.h
  src/blecon_zephyr_mutex.c
  src/blecon_zephyr_misc.c
)

if(BLECON_ZEPHYR_EXTRA)
include(blecon_zephyr-extra.cmake)
endif()

target_link_libraries(blecon_zephyr PRIVATE blecon)
target_include_directories(blecon_zephyr PRIVATE src)
target_include_directories(blecon_zephyr PUBLIC include)
target_include_directories(blecon_zephyr PRIVATE include/blecon_zephyr)
target_link_libraries(blecon_zephyr PUBLIC zephyr_interface)

zephyr_append_cmake_library(blecon)
zephyr_append_cmake_library(blecon_zephyr)
zephyr_include_directories(${ZEPHYR_CURRENT_MODULE_DIR}/include ${ZEPHYR_CURRENT_MODULE_DIR}/ports/zephyr/include)