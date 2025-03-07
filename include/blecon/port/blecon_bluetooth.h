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
#include "blecon/blecon_bluetooth_types.h"
#include "blecon/blecon_scheduler.h"

struct blecon_bluetooth_t;
struct blecon_bluetooth_advertising_set_t;
struct blecon_bluetooth_advertising_params_t;
struct blecon_bluetooth_advertising_data_t;
struct blecon_bluetooth_advertising_info_t;
struct blecon_bluetooth_connection_t;
struct blecon_bluetooth_connection_info_t;
struct blecon_bluetooth_l2cap_server_t;
struct blecon_bluetooth_gatt_server_t;

struct blecon_bluetooth_fn_t {
    void (*setup)(struct blecon_bluetooth_t* bluetooth);
    void (*shutdown)(struct blecon_bluetooth_t* bluetooth);

    struct blecon_bluetooth_advertising_set_t* (*advertising_set_new)(struct blecon_bluetooth_t* bluetooth);
    // selected tx power is updated by method below
    void (*advertising_set_update_params)(struct blecon_bluetooth_advertising_set_t* adv_set, struct blecon_bluetooth_advertising_params_t* params);
    void (*advertising_set_update_data)(struct blecon_bluetooth_advertising_set_t* adv_set, struct blecon_bluetooth_advertising_data_t* data);
    void (*advertising_set_start)(struct blecon_bluetooth_advertising_set_t* adv_set);
    void (*advertising_set_stop)(struct blecon_bluetooth_advertising_set_t* adv_set);
    void (*advertising_set_free)(struct blecon_bluetooth_advertising_set_t* adv_set);

    void (*get_address)(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_addr_t* bt_addr);

    struct blecon_bluetooth_l2cap_server_t* (*l2cap_server_new)(struct blecon_bluetooth_t* bluetooth, uint8_t psm);
    struct blecon_bluetooth_gatt_server_t* (*gatt_server_new)(struct blecon_bluetooth_t* bluetooth, const uint8_t* characteristic_uuid);

    void (*connection_get_info)(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_connection_info_t* info);
    void (*connection_get_power_info)(struct blecon_bluetooth_connection_t* connection, int8_t* tx_power, int8_t* rssi);
    void (*connection_disconnect)(struct blecon_bluetooth_connection_t* connection);
    struct blecon_bearer_t* (*connection_get_l2cap_server_bearer)(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_l2cap_server_t* l2cap_server);
    struct blecon_bearer_t* (*connection_get_gatt_server_bearer)(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_gatt_server_t* gatts);
    void (*connection_free)(struct blecon_bluetooth_connection_t* connection);

    void (*scan_start)(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_phy_mask_t phy_mask);
    void (*scan_stop)(struct blecon_bluetooth_t* bluetooth);
};

struct blecon_bluetooth_callbacks_t {
    void (*on_new_connection)(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_advertising_set_t* adv_set, void* user_data);
    void (*on_advertising_report)(struct blecon_bluetooth_t* bluetooth,
        const struct blecon_bluetooth_advertising_info_t* info,
        const struct blecon_bluetooth_advertising_data_t* data,
        void* user_data);
};

struct blecon_bluetooth_t {
    const struct blecon_bluetooth_fn_t* fns;
    struct blecon_scheduler_t* scheduler;
    const struct blecon_bluetooth_callbacks_t* callbacks;
    void* callbacks_user_data;
};

struct blecon_bluetooth_l2cap_server_t {
    struct blecon_bluetooth_t* bluetooth;
};

struct blecon_bluetooth_gatt_server_t {
    struct blecon_bluetooth_t* bluetooth;
};

struct blecon_bluetooth_advertising_params_t {
    struct blecon_bluetooth_addr_t bt_addr;
    bool legacy_pdu : 1;
    bool is_connectable : 1;
    uint8_t sid;
    int8_t tx_power;
    enum blecon_bluetooth_phy_t phy;
    uint32_t interval_0_625ms;
};

struct blecon_bluetooth_advertising_info_t {
    struct blecon_bluetooth_addr_t bt_addr;
    bool legacy_pdu : 1;
    bool is_connectable : 1;
    uint8_t sid;
    int8_t tx_power;
    int8_t rssi;
    enum blecon_bluetooth_phy_t phy;
};

struct blecon_bluetooth_advertising_data_t {
    const uint8_t* data;
    size_t data_sz;
};

struct blecon_bluetooth_advertising_set_t {
    struct blecon_bluetooth_t* bluetooth;
    void* user_data;
};

struct blecon_bluetooth_connection_callbacks_t {
    void (*on_disconnected)(struct blecon_bluetooth_connection_t* connection, void* user_data);

    // TODO? On PHY change requests/etc
};

struct blecon_bluetooth_connection_info_t {
    struct blecon_bluetooth_addr_t our_bt_addr;
    struct blecon_bluetooth_addr_t peer_bt_addr;
    bool is_central : 1;
    enum blecon_bluetooth_phy_t phy;
};

struct blecon_bluetooth_connection_t {
    struct blecon_bluetooth_t* bluetooth;
    const struct blecon_bluetooth_connection_callbacks_t* callbacks;
    void* callbacks_user_data;
};

static inline void blecon_bluetooth_init(struct blecon_bluetooth_t* bluetooth, const struct blecon_bluetooth_fn_t* fns) {
    bluetooth->fns = fns;
    bluetooth->callbacks = NULL;
    bluetooth->callbacks_user_data = NULL;
}

static inline void blecon_bluetooth_setup(struct blecon_bluetooth_t* bluetooth,
    struct blecon_scheduler_t* scheduler,
    const struct blecon_bluetooth_callbacks_t* callbacks, void* callbacks_user_data) {
    bluetooth->scheduler = scheduler;
    bluetooth->callbacks = callbacks;
    bluetooth->callbacks_user_data = callbacks_user_data;
    bluetooth->fns->setup(bluetooth);
}

static inline void blecon_bluetooth_shutdown(struct blecon_bluetooth_t* bluetooth) {
    bluetooth->fns->shutdown(bluetooth);
}

static inline struct blecon_bluetooth_advertising_set_t* blecon_bluetooth_advertising_set_new(struct blecon_bluetooth_t* bluetooth, void* user_data) {
    struct blecon_bluetooth_advertising_set_t* adv_set = bluetooth->fns->advertising_set_new(bluetooth);
    adv_set->bluetooth = bluetooth;
    adv_set->user_data = user_data;
    return adv_set;
}

static inline void blecon_bluetooth_advertising_set_update_params(struct blecon_bluetooth_advertising_set_t* adv_set, struct blecon_bluetooth_advertising_params_t* params) {
    adv_set->bluetooth->fns->advertising_set_update_params(adv_set, params);
}

static inline void blecon_bluetooth_advertising_set_update_data(struct blecon_bluetooth_advertising_set_t* adv_set, struct blecon_bluetooth_advertising_data_t* data) {
    adv_set->bluetooth->fns->advertising_set_update_data(adv_set, data);
}

static inline void blecon_bluetooth_advertising_set_start(struct blecon_bluetooth_advertising_set_t* adv_set) {
    adv_set->bluetooth->fns->advertising_set_start(adv_set);
}

static inline void blecon_bluetooth_advertising_set_stop(struct blecon_bluetooth_advertising_set_t* adv_set) {
    adv_set->bluetooth->fns->advertising_set_stop(adv_set);
}

static inline void blecon_bluetooth_advertising_set_free(struct blecon_bluetooth_advertising_set_t* adv_set) {
    adv_set->bluetooth->fns->advertising_set_free(adv_set);
}

static inline void blecon_bluetooth_get_address(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_addr_t* bt_addr) {
    bluetooth->fns->get_address(bluetooth, bt_addr);
}

static inline struct blecon_bluetooth_l2cap_server_t* blecon_bluetooth_l2cap_server_new(struct blecon_bluetooth_t* bluetooth, uint8_t psm) {
    struct blecon_bluetooth_l2cap_server_t* l2cap_server = bluetooth->fns->l2cap_server_new(bluetooth, psm);
    l2cap_server->bluetooth = bluetooth;
    return l2cap_server;
}

static inline struct blecon_bluetooth_gatt_server_t* blecon_bluetooth_gatt_server_new(struct blecon_bluetooth_t* bluetooth, const uint8_t* characteristic_uuid) {
    struct blecon_bluetooth_gatt_server_t* gatts = bluetooth->fns->gatt_server_new(bluetooth, characteristic_uuid);
    gatts->bluetooth = bluetooth;
    return gatts;
}

static inline void blecon_bluetooth_connection_set_callbacks(struct blecon_bluetooth_connection_t* connection, const struct blecon_bluetooth_connection_callbacks_t* callbacks, void* user_data) {
    connection->callbacks = callbacks;
    connection->callbacks_user_data = user_data;
}

static inline void blecon_bluetooth_connection_get_info(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_connection_info_t* info) {
    connection->bluetooth->fns->connection_get_info(connection, info);
}

static inline void blecon_bluetooth_connection_get_power_info(struct blecon_bluetooth_connection_t* connection, int8_t* tx_power, int8_t* rssi) {
    connection->bluetooth->fns->connection_get_power_info(connection, tx_power, rssi);
}

static inline void blecon_bluetooth_connection_disconnect(struct blecon_bluetooth_connection_t* connection) {
    connection->bluetooth->fns->connection_disconnect(connection);
}

static inline void blecon_bluetooth_connection_free(struct blecon_bluetooth_connection_t* connection) {
    connection->bluetooth->fns->connection_free(connection);
}

static inline struct blecon_bearer_t* blecon_bluetooth_connection_get_l2cap_server_bearer(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_l2cap_server_t* l2cap_server) {
    return connection->bluetooth->fns->connection_get_l2cap_server_bearer(connection, l2cap_server);
}

static inline struct blecon_bearer_t* blecon_bluetooth_connection_get_gatt_server_bearer(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_gatt_server_t* gatt_server) {
    return connection->bluetooth->fns->connection_get_gatt_server_bearer(connection, gatt_server);
}

static inline void blecon_bluetooth_scan_start(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_phy_mask_t phy_mask) {
    bluetooth->fns->scan_start(bluetooth, phy_mask);
}

static inline void blecon_bluetooth_scan_stop(struct blecon_bluetooth_t* bluetooth) {
    bluetooth->fns->scan_stop(bluetooth);
}

// Callbacks
static inline void blecon_bluetooth_on_new_connection(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_advertising_set_t* adv_set) {
    bluetooth->callbacks->on_new_connection(bluetooth, connection, adv_set, bluetooth->callbacks_user_data);
}

static inline void blecon_bluetooth_on_advertising_report(struct blecon_bluetooth_t* bluetooth, const struct blecon_bluetooth_advertising_info_t* info, const struct blecon_bluetooth_advertising_data_t* data) {
    bluetooth->callbacks->on_advertising_report(bluetooth, info, data, bluetooth->callbacks_user_data);
}

static inline void blecon_bluetooth_connection_on_disconnected(struct blecon_bluetooth_connection_t* connection) {
    connection->callbacks->on_disconnected(connection, connection->callbacks_user_data);
}

// Used by implementations
static inline void blecon_bluetooth_connection_init(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_t* bluetooth) {
    connection->bluetooth = bluetooth;
    connection->callbacks = NULL;
    connection->callbacks_user_data = NULL;
}

#ifdef __cplusplus
}
#endif
