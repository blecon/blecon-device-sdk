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
struct blecon_bearer_t* blecon_zephyr_bluetooth_l2cap_server_as_bearer(struct blecon_bluetooth_l2cap_server_t* l2cap_server);
void blecon_zephyr_bluetooth_l2cap_server_free(struct blecon_bluetooth_l2cap_server_t* l2cap_server);


#ifdef __cplusplus
}
#endif
