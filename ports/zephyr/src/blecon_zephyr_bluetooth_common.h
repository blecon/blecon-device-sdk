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
#include "blecon_zephyr/blecon_zephyr_gatts_bearer.h"

#include "zephyr/bluetooth/bluetooth.h"
#include "zephyr/bluetooth/conn.h"

struct blecon_event_loop_t;
struct blecon_zephyr_bluetooth_connection_t;
struct bt_conn;

struct blecon_zephyr_bluetooth_advertising_set_t {
    struct blecon_bluetooth_advertising_set_t set;
    int32_t identity;
    struct bt_le_ext_adv* handle;
};

struct blecon_zephyr_bluetooth_connection_t {
    struct blecon_bluetooth_connection_t connection;
    // Referenced from the connection to connection_free, past the link's disconnection: Zephyr reuses no bt_conn a
    // reference holds and keeps its fields, so the connection's reads stay on its own link. NULL while there is none
    struct bt_conn* conn;
    // The link's ATT MTU, from Zephyr's att_mtu_updated: the GATT server bearers' MTU, which Zephyr itself reads as 0
    // from the moment the link leaves its connected state (a disconnection requested, or the controller's processed)
    uint16_t att_mtu;
};

struct blecon_zephyr_bluetooth_t {
    struct blecon_bluetooth_t bluetooth;
    struct blecon_event_loop_t* event_loop;
    struct blecon_zephyr_bluetooth_advertising_set_t adv_sets[BLECON_MAX_ADVERTISING_SETS];
    size_t adv_sets_count;

    uint8_t bt_addr[BLECON_BLUETOOTH_ADDR_SZ];

    struct blecon_zephyr_gatts_t gatts;

    struct blecon_zephyr_bluetooth_connection_t connection;

    struct blecon_task_t setup_complete_task; // Fires on_setup_complete deferred out of setup()
    bool setup_complete_raised;               // Guards on_setup_complete to fire at most once
};

#ifdef __cplusplus
}
#endif
