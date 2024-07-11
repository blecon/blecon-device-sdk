/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "blecon/blecon_defs.h"

#include "blecon/port/blecon_bluetooth.h"
#include "blecon_nrf5/blecon_nrf5_gatts_bearer.h"

#define BLECON_NRF5_BLUETOOTH_MAX_ADV_SETS 3

struct blecon_nrf5_bluetooth_advertising_data_t {
    struct blecon_bluetooth_advertising_set_t sets[BLECON_NRF5_BLUETOOTH_MAX_ADV_SETS];
    size_t sets_count;
    size_t current_set;
    size_t next_set;
    uint8_t handle;
    ble_gap_addr_t base_addr;
};

struct blecon_nrf5_bluetooth_t {
    struct blecon_bluetooth_t bluetooth;
    uint16_t conn_handle; // Handle of the current connection
    enum blecon_bluetooth_phy_t conn_phy;
    bool advertising;
    struct blecon_nrf5_bluetooth_advertising_data_t adv_data;
    struct blecon_nrf5_gatts_t gatts;
};

#ifdef __cplusplus
}
#endif
