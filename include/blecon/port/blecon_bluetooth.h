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

struct blecon_bluetooth_advertising_set_t {
    uint8_t adv_data[31];
    size_t adv_data_sz;
    bool is_connectable : 1;
    enum blecon_bluetooth_phy_t phy;
};

struct blecon_bluetooth_t;
struct blecon_bluetooth_l2cap_server_t;
struct blecon_bluetooth_gatts_bearer_t;

struct blecon_bluetooth_fn_t {
    void (*setup)(struct blecon_bluetooth_t* bluetooth);
    int32_t (*advertising_tx_power)(struct blecon_bluetooth_t* bluetooth);
    int32_t (*connection_tx_power)(struct blecon_bluetooth_t* bluetooth);
    int32_t (*connection_rssi)(struct blecon_bluetooth_t* bluetooth);
    enum blecon_bluetooth_phy_t (*connection_phy)(struct blecon_bluetooth_t* bluetooth);
    void (*advertising_set_data)(
        struct blecon_bluetooth_t* bluetooth,
        const struct blecon_bluetooth_advertising_set_t* adv_sets,
        size_t adv_sets_count
    );
    bool (*advertising_start)(struct blecon_bluetooth_t* bluetooth, uint32_t adv_interval_ms);
    void (*advertising_stop)(struct blecon_bluetooth_t* bluetooth);
    void (*rotate_mac_address)(struct blecon_bluetooth_t* bluetooth);
    void (*get_mac_address)(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_addr_t* bt_mac_addr);
    void (*disconnect)(struct blecon_bluetooth_t* bluetooth);

    struct blecon_bluetooth_l2cap_server_t* (*l2cap_server_new)(struct blecon_bluetooth_t* bluetooth, uint8_t psm);
    struct blecon_bearer_t* (*l2cap_server_as_bearer)(struct blecon_bluetooth_l2cap_server_t* l2cap_server);
    void (*l2cap_server_free)(struct blecon_bluetooth_l2cap_server_t* l2cap_server);

    struct blecon_bluetooth_gatts_bearer_t* (*gatts_bearer_new)(struct blecon_bluetooth_t* bluetooth, const uint8_t* characteristic_uuid);
    struct blecon_bearer_t* (*gatts_bearer_as_bearer)(struct blecon_bluetooth_gatts_bearer_t* gatts_bearer);
    void (*gatts_bearer_free)(struct blecon_bluetooth_gatts_bearer_t* gatts_bearer);
};

struct blecon_bluetooth_callbacks_t {
    void (*on_connected)(struct blecon_bluetooth_t* bluetooth, void* user_data);
    void (*on_disconnected)(struct blecon_bluetooth_t* bluetooth, void* user_data);
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

struct blecon_bluetooth_gatts_bearer_t {
    struct blecon_bluetooth_t* bluetooth;
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

static inline int32_t blecon_bluetooth_advertising_tx_power(struct blecon_bluetooth_t* bluetooth) {
    return bluetooth->fns->advertising_tx_power(bluetooth);
}

static inline int32_t blecon_bluetooth_connection_tx_power(struct blecon_bluetooth_t* bluetooth) {
    return bluetooth->fns->connection_tx_power(bluetooth);
}

static inline int32_t blecon_bluetooth_connection_rssi(struct blecon_bluetooth_t* bluetooth) {
    return bluetooth->fns->connection_rssi(bluetooth);
}

static inline enum blecon_bluetooth_phy_t blecon_bluetooth_connection_phy(struct blecon_bluetooth_t* bluetooth) {
    return bluetooth->fns->connection_phy(bluetooth);
}

static inline void blecon_bluetooth_advertising_set_data(
    struct blecon_bluetooth_t* bluetooth,
    const struct blecon_bluetooth_advertising_set_t* adv_sets,
    size_t adv_sets_count
) {
    return bluetooth->fns->advertising_set_data(bluetooth, adv_sets, adv_sets_count);
}

static inline bool blecon_bluetooth_advertising_start(struct blecon_bluetooth_t* bluetooth, uint32_t adv_interval_ms) {
    return bluetooth->fns->advertising_start(bluetooth, adv_interval_ms);
}

static inline void blecon_bluetooth_advertising_stop(struct blecon_bluetooth_t* bluetooth) {
    return bluetooth->fns->advertising_stop(bluetooth);
}

static inline void blecon_bluetooth_rotate_mac_address(struct blecon_bluetooth_t* bluetooth) {
    bluetooth->fns->rotate_mac_address(bluetooth);
}

static inline void blecon_bluetooth_get_mac_address(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_addr_t* bt_mac_addr) {
    bluetooth->fns->get_mac_address(bluetooth, bt_mac_addr);
}

static inline void blecon_bluetooth_disconnect(struct blecon_bluetooth_t* bluetooth) {
    bluetooth->fns->disconnect(bluetooth);
}

static inline struct blecon_bearer_t* blecon_bluetooth_l2cap_server_as_bearer(struct blecon_bluetooth_l2cap_server_t* l2cap_server) {
    return l2cap_server->bluetooth->fns->l2cap_server_as_bearer(l2cap_server);
}

static inline struct blecon_bluetooth_l2cap_server_t* blecon_bluetooth_l2cap_server_new(struct blecon_bluetooth_t* bluetooth, uint8_t psm) {
    struct blecon_bluetooth_l2cap_server_t* l2cap_server = bluetooth->fns->l2cap_server_new(bluetooth, psm);
    l2cap_server->bluetooth = bluetooth;
    return l2cap_server;
}

static inline void blecon_bluetooth_l2cap_server_free(struct blecon_bluetooth_l2cap_server_t* l2cap_server) {
    l2cap_server->bluetooth->fns->l2cap_server_free(l2cap_server);
}

static inline struct blecon_bearer_t* blecon_bluetooth_gatts_bearer_as_bearer(struct blecon_bluetooth_gatts_bearer_t* gatts_bearer) {
    return gatts_bearer->bluetooth->fns->gatts_bearer_as_bearer(gatts_bearer);
}

static inline struct blecon_bluetooth_gatts_bearer_t* blecon_bluetooth_gatts_bearer_new(struct blecon_bluetooth_t* bluetooth, const uint8_t* characteristic_uuid) {
    struct blecon_bluetooth_gatts_bearer_t* gatts_bearer = bluetooth->fns->gatts_bearer_new(bluetooth, characteristic_uuid);
    gatts_bearer->bluetooth = bluetooth;
    return gatts_bearer;
}

static inline void blecon_bluetooth_gatts_bearer_free(struct blecon_bluetooth_gatts_bearer_t* gatts_bearer) {
    gatts_bearer->bluetooth->fns->gatts_bearer_free(gatts_bearer);
}

// Callbacks
static inline void blecon_bluetooth_on_connected(struct blecon_bluetooth_t* bluetooth) {
    bluetooth->callbacks->on_connected(bluetooth, bluetooth->callbacks_user_data);
}

static inline void blecon_bluetooth_on_disconnected(struct blecon_bluetooth_t* bluetooth) {
    bluetooth->callbacks->on_disconnected(bluetooth, bluetooth->callbacks_user_data);
}

#ifdef __cplusplus
}
#endif
