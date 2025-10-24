# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_SOC_NRF54L15_CPUAPP)
  board_runner_args(jlink "--device=nrf54l15_m33" "--speed=1000")
elseif(CONFIG_SOC_NRF54L15_CPUFLPR)
  set(JLINKSCRIPTFILE ${CMAKE_CURRENT_LIST_DIR}/support/nrf54l15_cpuflpr.JLinkScript)
  board_runner_args(jlink "--device=RISC-V" "--speed=4000" "-if SW" "--tool-opt=-jlinkscriptfile ${JLINKSCRIPTFILE}")
endif()

if(BOARD_MOKOSMART_L02S_BCN_NRF54L15_CPUAPP_NS)
  set(TFM_PUBLIC_KEY_FORMAT "full")
endif()

if(CONFIG_TFM_FLASH_MERGED_BINARY)
  set_property(TARGET runners_yaml_props_target PROPERTY hex_file tfm_merged.hex)
endif()

include(${ZEPHYR_BASE}/boards/common/nrfutil.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
