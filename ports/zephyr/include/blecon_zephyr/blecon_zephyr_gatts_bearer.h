/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"
#include "stddef.h"
#include "blecon/blecon_bearer.h"
#include "blecon/blecon_scheduler.h"
#include "blecon/port/blecon_bluetooth.h"

#include "zephyr/bluetooth/gatt.h"

#define BLECON_ZEPHYR_MAX_GATTS_BEARERS 2

struct blecon_zephyr_bluetooth_t;

struct blecon_zephyr_bluetooth_gatt_server_bearer_t {
    struct blecon_bluetooth_gatt_server_t gatt_server;
    struct blecon_zephyr_bluetooth_t* bluetooth;
    struct blecon_bearer_t bearer;
    // struct bt_uuid_128 characteristic_uuid;
    const struct bt_gatt_attr* attr;
    bool connected;
    struct blecon_task_t close_task;
};

struct blecon_zephyr_gatts_t {
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t bearers[BLECON_ZEPHYR_MAX_GATTS_BEARERS];
    size_t bearers_count;
};

void blecon_zephyr_bluetooth_gatt_server_init(struct blecon_zephyr_bluetooth_t* zephyr_bluetooth);

struct blecon_bluetooth_gatt_server_t* blecon_zephyr_bluetooth_gatt_server_new(struct blecon_bluetooth_t* bluetooth, const uint8_t* characteristic_uuid);
struct blecon_bearer_t* blecon_zephyr_bluetooth_connection_get_gatt_server_bearer(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_gatt_server_t* gatt_server);

#ifdef __cplusplus
}
#endif
