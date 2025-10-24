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
#include "blecon/blecon_defs.h"

enum blecon_bluetooth_addr_type_t {
    blecon_bluetooth_addr_type_public = 0,
    blecon_bluetooth_addr_type_random = 1
};

struct blecon_bluetooth_addr_t {
    uint8_t bytes[BLECON_BLUETOOTH_ADDR_SZ];
    uint8_t addr_type;
} __attribute__((packed));

enum blecon_bluetooth_phy_t {
    blecon_bluetooth_phy_1m = 0,
    blecon_bluetooth_phy_2m = 1,
    blecon_bluetooth_phy_coded = 2
};

bool blecon_bluetooth_addr_eq(const struct blecon_bluetooth_addr_t addr, const struct blecon_bluetooth_addr_t other);

struct blecon_bluetooth_phy_mask_t {
    bool phy_1m : 1;
    bool phy_2m : 1;
    bool phy_coded : 1;
};

struct blecon_bluetooth_bearer_mask_t {
    bool bearer_gatt : 1;
    bool bearer_l2cap : 1;
};

struct blecon_bluetooth_advertising_info_t {
    struct blecon_bluetooth_addr_t bt_addr;
    bool legacy_pdu : 1;
    bool is_connectable : 1;
    bool is_scan_response : 1;
    uint8_t sid : 4;
    int8_t tx_power;
    int8_t rssi;
    enum blecon_bluetooth_phy_t phy;
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

struct blecon_bluetooth_advertising_data_t {
    const uint8_t* data;
    size_t data_sz;
};

#ifdef __cplusplus
}
#endif
