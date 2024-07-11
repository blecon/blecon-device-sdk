/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "blecon/blecon_bearer.h"
#include "blecon/blecon_buffer_queue.h"
#include "blecon/port/blecon_bluetooth.h"
#include "stdbool.h"
#include "stdint.h"
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"

#define BLECON_NRF5_BLUETOOTH_L2CAP_SERVERS_MAX 2
#define BLECON_NRF5_BLUETOOTH_L2CAP_MPS 247 // Assuming DLE are supported, this is as much as we can send in a PDU (251 bytes - 4-byte L2CAP headers)
#define BLECON_NRF5_BLUETOOTH_L2CAP_MTU (BLECON_NRF5_BLUETOOTH_L2CAP_MPS - 2) // This is how much we can send in a SDU so that it's not fragmented over multiple PDUs
#define BLECON_NRF5_BLUETOOTH_L2CAP_RX_EXTRA_CREDITS 1
#define BLECON_NRF5_BLUETOOTH_L2CAP_TX_BUF_MAX 2
#define BLECON_NRF5_BLUETOOTH_L2CAP_RX_BUF_MAX 2

struct blecon_bluetooth_l2cap_server_t* blecon_nrf5_bluetooth_l2cap_server_new(struct blecon_bluetooth_t* bluetooth, uint8_t psm);
struct blecon_bearer_t* blecon_nrf5_bluetooth_l2cap_server_as_bearer(struct blecon_bluetooth_l2cap_server_t* l2cap_server);
void blecon_nrf5_bluetooth_l2cap_server_free(struct blecon_bluetooth_l2cap_server_t* l2cap_server);

#ifdef __cplusplus
}
#endif
