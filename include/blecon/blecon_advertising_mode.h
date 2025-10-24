/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advertising mode enumeration
 * 
 * Controls the advertising performance and power consumption trade-off.
 */
enum blecon_advertising_mode_t {
    blecon_advertising_mode_high_performance,   /**< High performance - 100ms period, higher power consumption */
    blecon_advertising_mode_balanced,           /**< Balanced - alternate between 100ms and 1s advertising period when requesting connection, otherwise 1s period, balances power consumption with performance */
    blecon_advertising_mode_ultra_low_power,    /**< Ultra low power - only advertise (100ms/1s alternating) when requesting connection, lowest power consumption */
};

#ifdef __cplusplus
}
#endif
