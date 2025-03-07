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

#ifdef __cplusplus
}
#endif
