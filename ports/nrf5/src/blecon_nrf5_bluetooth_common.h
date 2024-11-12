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

// struct blecon_nrf5_bluetooth_advertising_data_t {
//     struct blecon_bluetooth_advertising_set_t sets[BLECON_NRF5_BLUETOOTH_MAX_ADV_SETS];
//     size_t sets_count;
//     size_t current_set;
//     size_t next_set;
//     uint8_t handle;
//     ble_gap_addr_t base_addr;
// };

struct blecon_nrf5_bluetooth_advertising_set_t {
    struct blecon_bluetooth_advertising_set_t set;
    struct blecon_bluetooth_advertising_params_t params;
    uint8_t data_sz;
    uint8_t data[31];
    bool is_advertising;
};

struct blecon_nrf5_bluetooth_advertising_t {
    struct blecon_nrf5_bluetooth_advertising_set_t sets[BLECON_MAX_ADVERTISING_SETS];
    size_t sets_count;
    size_t set_current;
    size_t set_next;
    size_t sets_advertising;
    uint8_t handle;
};

struct blecon_nrf5_bluetooth_connection_t {
    struct blecon_bluetooth_connection_t connection;
    uint16_t handle;
    struct blecon_bluetooth_addr_t our_bt_addr;
    struct blecon_bluetooth_addr_t peer_bt_addr;
    enum blecon_bluetooth_phy_t phy;
};

struct blecon_nrf5_bluetooth_t {
    struct blecon_bluetooth_t bluetooth;
    struct blecon_nrf5_bluetooth_advertising_t advertising;
    struct blecon_nrf5_bluetooth_connection_t connection;
    struct blecon_nrf5_gatts_t gatts;
#ifdef S140
    uint8_t scan_buffer[BLE_GAP_SCAN_BUFFER_EXTENDED_MIN];
    ble_gap_scan_params_t scan_params;
    bool is_scanning;
#endif
};

#ifdef __cplusplus
}
#endif
