# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BLECON)

if(EXISTS  "${CMAKE_CURRENT_SOURCE_DIR}/blecon_zephyr-extra.cmake")
  set(BLECON_ZEPHYR_HAS_EXTRA TRUE)
else()
  set(BLECON_ZEPHYR_HAS_EXTRA FALSE)
endif()

cmake_dependent_option(BLECON_ZEPHYR_EXTRA "Compile extra Zephyr port" ON "BLECON_ZEPHYR_HAS_EXTRA" OFF)

add_library(blecon_zephyr STATIC)

target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_mutex.c
  src/blecon_zephyr_misc.c
  src/blecon_zephyr_error.c
)

if(CONFIG_BLECON_PORT_EVENT_LOOP)
target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_event_loop.c
)
endif()

if(CONFIG_BLECON_PORT_CRYPTO)
target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_crypto.c
)
endif()

# Always compile as it includes a dummy implementation if NFC is not present
target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_nfc.c
)

if(CONFIG_BLECON_PORT_NVM)
target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_nvm.c
)
endif()

if(CONFIG_BLECON_PORT_BLUETOOTH)
target_sources(blecon_zephyr PRIVATE
src/blecon_zephyr_bluetooth.c
src/blecon_zephyr_l2cap_bearer.c
src/blecon_zephyr_l2cap_server.c
src/blecon_zephyr_gatts_bearer.c
)
endif()

if(CONFIG_BLECON_PORT_UART)
target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_ext_modem_uart_transport.c
)
endif()

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

endif()
