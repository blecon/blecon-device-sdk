# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BLECON)

add_library(blecon_zephyr STATIC)

target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_mutex.c
  src/blecon_zephyr_misc.c
  src/blecon_zephyr_error.c
  src/blecon_zephyr.c
)

if(CONFIG_BLECON_PORT_EVENT_LOOP)
target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_event_loop.c
)
endif()

if(CONFIG_BLECON_PORT_TIMER)
target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_timer.c
)
endif()

if(CONFIG_BLECON_PORT_CRYPTO)
target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_crypto.c
)
# Add mbedTLS include directories for PSA crypto headers
if(CONFIG_MBEDTLS)
  # Zephyr's mbedtls module handles include paths through the mbedTLS target
  if(TARGET mbedTLS)
    # Get the INTERFACE include directories from mbedTLS and add them
    get_target_property(MBEDTLS_INCLUDES mbedTLS INTERFACE_INCLUDE_DIRECTORIES)
    if(MBEDTLS_INCLUDES)
      target_include_directories(blecon_zephyr PRIVATE ${MBEDTLS_INCLUDES})
    endif()
  endif()
  # Always add the standard mbedTLS paths as fallback
  target_include_directories(blecon_zephyr PRIVATE
    ${ZEPHYR_MBEDTLS_MODULE_DIR}/include
    ${ZEPHYR_MBEDTLS_MODULE_DIR}/include/library
    ${ZEPHYR_MBEDTLS_MODULE_DIR}/library
  )
endif()
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

if(CONFIG_BLECON_PORT_OTP)
target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_otp.c
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

if(CONFIG_BLECON_TRACING)
target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_tracing.c
)
endif()

if(CONFIG_BLECON_TRACING_BACKEND_INBAND)
# The sticky side-buffer sizing defaults live in blecon_trace_ram_transport.h; on
# Zephyr Kconfig is authoritative, so override them for every consumer of it.
target_compile_definitions(blecon PUBLIC
  BLECON_TRACE_RAM_STICKY_MAX_SLOTS=${CONFIG_BLECON_TRACING_INBAND_STICKY_MAX_SLOTS}
  BLECON_TRACE_RAM_STICKY_POOL_SIZE=${CONFIG_BLECON_TRACING_INBAND_STICKY_POOL_SIZE}
  BLECON_TRACE_RAM_STICKY_SCRATCH_SIZE=${CONFIG_BLECON_TRACING_INBAND_STICKY_SCRATCH_SIZE}
)
endif()

if(CONFIG_BLECON_TRACING_SERIAL_SINK)
target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_trace_serial_transport.c
)
endif()

if(CONFIG_BLECON_TRACING_BACKEND_SWO)
target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_trace_swo_transport.c
)
# On Nordic SoCs the vendor MDK's SystemInit() only selects the trace mux
# (CLOCK->TRACECONFIG.TRACEMUX) when ENABLE_SWO is defined; without it the ITM/TPIU
# bring-up succeeds and the stream silently goes nowhere. Zephyr's own SWO log
# backend opts in the same way, so enabling both just defines it twice.
if(CONFIG_SOC_FAMILY_NORDIC_NRF)
  zephyr_compile_definitions(ENABLE_SWO)
endif()
endif()

if(CONFIG_BLECON_MEMFAULT)
target_sources(blecon_zephyr PRIVATE
  src/blecon_zephyr_memfault.c
  src/blecon_zephyr_memfault_ota.c
)
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
