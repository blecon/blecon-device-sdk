/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "blecon/blecon_defs.h"
#include "blecon/blecon_scheduler.h"

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
    struct blecon_bluetooth_advertising_params_t started_params; // Those it was last started with: a connection's on it
    uint8_t data_sz;
    uint8_t data[31]; // The SoftDevice's buffer while the set is on air, which names the set in its events
    bool is_advertising;
};

// The SoftDevice has one advertising set, which the port gives each advertising set in turn: a round starts every
// interval, at the round timeout, and advertises each set once (one advertising event), in index order, each started
// as the one before ends (BLE_GAP_EVT_ADV_SET_TERMINATED). All of it in thread mode
struct blecon_nrf5_bluetooth_advertising_t {
    struct blecon_nrf5_bluetooth_advertising_set_t sets[BLECON_MAX_ADVERTISING_SETS];
    size_t sets_count;
    size_t sets_advertising;
    size_t set_current;                    // The set last started on the SoftDevice
    bool on_air;                           // set_current is on air: from its start to its end (its event, a connection on it, or a stop)
    struct blecon_timeout_t round_timeout; // Queued while any set advertises
    uint8_t handle;
};

struct blecon_nrf5_bluetooth_connection_t {
    struct blecon_bluetooth_connection_t connection;
    uint16_t handle;
    struct blecon_bluetooth_device_id_t our_device_id;
    struct blecon_bluetooth_device_id_t peer_device_id;
    enum blecon_bluetooth_phy_t phy;
};

struct blecon_nrf5_bluetooth_t {
    struct blecon_bluetooth_t bluetooth;
    struct blecon_nrf5_bluetooth_advertising_t advertising;
    struct blecon_nrf5_bluetooth_connection_t connection;
    struct blecon_nrf5_gatts_t gatts;
    struct blecon_task_t setup_complete_task; // Fires on_setup_complete deferred out of setup()
    bool setup_complete_raised;               // Guards on_setup_complete to fire at most once
#ifdef S140
    uint8_t scan_buffer[BLE_GAP_SCAN_BUFFER_EXTENDED_MIN];
    ble_gap_scan_params_t scan_params;
    bool is_scanning;
#endif
};

#ifdef __cplusplus
}
#endif
