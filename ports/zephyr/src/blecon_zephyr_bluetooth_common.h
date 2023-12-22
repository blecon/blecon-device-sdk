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
struct blecon_zephyr_bluetooth_central_connection_t;
struct bt_conn;

struct blecon_zephyr_bluetooth_advertising_set_t {
    struct blecon_bluetooth_advertising_set_t set;
    uint8_t identity;
    struct bt_le_ext_adv* handle;
};

struct blecon_zephyr_bluetooth_t {
    struct blecon_bluetooth_t bluetooth;
    struct blecon_event_loop_t* event_loop;
    bool advertising;
    struct blecon_zephyr_bluetooth_advertising_set_t adv_sets[BLECON_MAX_ADVERTISING_SETS];
    size_t adv_sets_count;
    uint32_t adv_interval_ms;

    uint8_t adv_base_addr[BLECON_BLUETOOTH_ADDR_SZ];

    struct blecon_zephyr_gatts_t gatts;

    struct bt_conn* conn;
};

#ifdef __cplusplus
}
#endif
