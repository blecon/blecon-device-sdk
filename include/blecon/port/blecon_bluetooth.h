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

    // An advertising set lives until it is freed or until the Bluetooth instance it was created on is shut down,
    // whichever comes first, and must not be used after either: shutdown() releases the sets still allocated,
    // stopping those advertising. Ports whose sets are static (Zephyr, nRF5) have an empty shutdown(), which
    // neither stops nor releases a set, and freeing a set there is a fatal error.
    struct blecon_bluetooth_advertising_set_t* (*advertising_set_new)(struct blecon_bluetooth_t* bluetooth);
    // selected tx power is updated by method below
    void (*advertising_set_update_params)(struct blecon_bluetooth_advertising_set_t* adv_set, struct blecon_bluetooth_advertising_params_t* params);
    void (*advertising_set_update_data)(struct blecon_bluetooth_advertising_set_t* adv_set, struct blecon_bluetooth_advertising_data_t* data);
    void (*advertising_set_start)(struct blecon_bluetooth_advertising_set_t* adv_set);
    // Stopping a set that is not advertising (never started, already stopped, or ended by a connection) has no effect
    void (*advertising_set_stop)(struct blecon_bluetooth_advertising_set_t* adv_set);
    // The set must not be advertising
    void (*advertising_set_free)(struct blecon_bluetooth_advertising_set_t* adv_set);

    void (*get_address)(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_addr_t* bt_addr);

    // There is no per-server free: a server lives until the Bluetooth instance it was created on is shut down
    // and must not be used after that shutdown(); an implementation that can release a server's resources
    // does so in shutdown(). Its bearers end with it: a connection still held after the shutdown has none
    // on it to use, and a port may raise nothing more for them, not even their close (the host port ends
    // them there, as the connection's free would). A connection has bearers on the servers that exist
    // when it is made, and on no later one: connection_get_*_server_bearer() must not be asked for one.
    struct blecon_bluetooth_l2cap_server_t* (*l2cap_server_new)(struct blecon_bluetooth_t* bluetooth, uint8_t psm);
    struct blecon_bluetooth_gatt_server_t* (*gatt_server_new)(struct blecon_bluetooth_t* bluetooth, const uint8_t* characteristic_uuid);

    void (*connection_get_info)(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_connection_info_t* info);
    // *rssi is this side's RSSI for the connection, or BLECON_BLUETOOTH_RSSI_UNKNOWN when the
    // port has no reading (none taken yet, the controller reported none, or the connection's link is
    // gone: disconnected, or ended by the controller ahead of the disconnection the port raises; a port
    // whose stack names no link once a disconnection is under way reads none from then; a port may also
    // read none when its stack refuses the read) — never 0.
    void (*connection_get_power_info)(struct blecon_bluetooth_connection_t* connection, int8_t* tx_power, int8_t* rssi);
    void (*connection_disconnect)(struct blecon_bluetooth_connection_t* connection);
    struct blecon_bearer_t* (*connection_get_l2cap_server_bearer)(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_l2cap_server_t* l2cap_server);
    struct blecon_bearer_t* (*connection_get_gatt_server_bearer)(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_gatt_server_t* gatts);
    void (*connection_free)(struct blecon_bluetooth_connection_t* connection);

    void (*scan_start)(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_phy_mask_t phy_mask, bool active_scan);
    void (*scan_stop)(struct blecon_bluetooth_t* bluetooth);
};

struct blecon_bluetooth_callbacks_t {
    void (*on_new_connection)(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_advertising_set_t* adv_set, void* user_data);
    // info->rssi is the controller's value as-is: BLECON_BLUETOOTH_RSSI_UNKNOWN when it reported none.
    void (*on_advertising_report)(struct blecon_bluetooth_t* bluetooth, const struct blecon_bluetooth_advertising_info_t* info, const struct blecon_bluetooth_advertising_data_t* data, void* user_data);

    // Raised once per blecon_bluetooth_setup(), when the stack is ready to use: get_address, and
    // every call that drives the controller (advertising, scanning, connecting), may only be issued
    // from this point on, while advertising sets and L2CAP / GATT servers may be created as soon as
    // setup() has returned (the advertiser creates them there). Ports raise it deferred (from the
    // scheduler, or a watcher of their own on the event loop), never synchronously inside setup().
    void (*on_setup_complete)(struct blecon_bluetooth_t* bluetooth, void* user_data);
    // Raised if the stack becomes unavailable after setup completed (controller reset / HCI
    // transport loss). Maps onto the shared cross-port "Bluetooth powered off/disabled" concept;
    // the stack performs no auto-recovery. A port may treat that loss as fatal instead and never
    // raise it: the Linux host port exits the process, on either backend (spec §3.7).
    void (*on_stack_disabled)(struct blecon_bluetooth_t* bluetooth, void* user_data);
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

struct blecon_bluetooth_advertising_set_t {
    struct blecon_bluetooth_t* bluetooth;
    void* user_data;
};

struct blecon_bluetooth_connection_callbacks_t {
    void (*on_disconnected)(struct blecon_bluetooth_connection_t* connection, void* user_data);

    // TODO? On PHY change requests/etc
};

struct blecon_bluetooth_connection_info_t {
    struct blecon_bluetooth_device_id_t our_device_id;
    struct blecon_bluetooth_device_id_t peer_device_id;
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

static inline void blecon_bluetooth_setup(struct blecon_bluetooth_t* bluetooth, struct blecon_scheduler_t* scheduler, const struct blecon_bluetooth_callbacks_t* callbacks, void* callbacks_user_data) {
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

static inline void blecon_bluetooth_scan_start(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_phy_mask_t phy_mask, bool active_scan) {
    bluetooth->fns->scan_start(bluetooth, phy_mask, active_scan);
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

static inline void blecon_bluetooth_on_setup_complete(struct blecon_bluetooth_t* bluetooth) {
    bluetooth->callbacks->on_setup_complete(bluetooth, bluetooth->callbacks_user_data);
}

static inline void blecon_bluetooth_on_stack_disabled(struct blecon_bluetooth_t* bluetooth) {
    bluetooth->callbacks->on_stack_disabled(bluetooth, bluetooth->callbacks_user_data);
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
