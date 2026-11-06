# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

# If BLECON_JLINK_REMOTE_SERVER_IP is set, use J-Link remote server
if(DEFINED ENV{BLECON_JLINK_REMOTE_SERVER_IP})
    message(STATUS "BOARD_FLASH_RUNNER = ${BOARD_FLASH_RUNNER}")
    set(BOARD_FLASH_RUNNER "jlink")
    set(BOARD_DEBUG_RUNNER "jlink")
    macro(app_set_runner_args)
        if(${runner} STREQUAL "jlink")
            board_runner_args(jlink "--id=$ENV{BLECON_JLINK_REMOTE_SERVER_IP}")
        endif()
    endmacro()
endif()
