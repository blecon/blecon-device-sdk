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
#include "blecon/port/blecon_bluetooth.h"

struct blecon_bluetooth_l2cap_server_t* blecon_zephyr_bluetooth_l2cap_server_new(struct blecon_bluetooth_t* bluetooth, uint8_t psm);
struct blecon_bearer_t* blecon_zephyr_bluetooth_connection_get_l2cap_server_bearer(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_l2cap_server_t* l2cap_server);


#ifdef __cplusplus
}
#endif
