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
#include "blecon/blecon_scheduler.h"
#include "blecon/port/blecon_bluetooth.h"
#include "stdbool.h"
#include "stdint.h"
#include "stddef.h"
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"

#define BLECON_NRF5_MAX_GATTS_BEARERS 2

// The most notifications the port has the SoftDevice hold for the link at once: two per bearer, the most the fragmentation
// layer above a bearer keeps unsent. Not a share per bearer (a closed bearer's notifications in flight take room too), and
// past it a notification waits in its bearer's held queue, so the bound is a throughput choice, never a loss
#define BLECON_NRF5_GATTS_IN_FLIGHT_MAX (BLECON_NRF5_MAX_GATTS_BEARERS * 2)

struct blecon_nrf5_bluetooth_t;

struct blecon_nrf5_gatts_bearer_t {
    struct blecon_bluetooth_gatt_server_t gatt_server;
    struct blecon_bearer_t bearer;
    ble_gatts_char_handles_t handles;
    bool connected;
    struct blecon_buffer_queue_t held; // The notifications the SoftDevice has not taken, oldest first: refused, or past
                                       // BLECON_NRF5_GATTS_IN_FLIGHT_MAX. Empty while closed
    struct blecon_task_t close_task;
};

struct blecon_nrf5_gatts_t {
    struct blecon_nrf5_gatts_bearer_t bearers[BLECON_NRF5_MAX_GATTS_BEARERS];
    size_t bearers_count;
    uint16_t service_handle;
    uint8_t base_service_characteristics_uuid;
    // uint16_t connection_handle;
    size_t att_mtu;
    // The notifications the SoftDevice has taken and not yet reported sent (BLE_GATTS_EVT_HVN_TX_COMPLETE), in the order it
    // took them, which is the order it sends them in: each its bearer's, NULL once that bearer has closed (assumed: the
    // SoftDevice still sends and counts those after the peer turns notifications off). Every notification on the link is
    // the port's. Cleared at the disconnection, where the SoftDevice drops what it holds
    struct blecon_nrf5_gatts_bearer_t* in_flight[BLECON_NRF5_GATTS_IN_FLIGHT_MAX];
    size_t in_flight_first;
    size_t in_flight_count;
    size_t submit_first; // The bearer the next offer of every bearer's held notifications starts at, so none waits behind another's
    // Queued while a notification refused with NRF_ERROR_INVALID_STATE is held: no event ends that refusal's one cause on
    // a live link and an open bearer, an ATT_MTU exchange
    struct blecon_timeout_t retry_timeout;
    bool retry_queued;
};

void blecon_nrf5_bluetooth_gatt_server_init(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth);
void blecon_nrf5_bluetooth_gatt_server_setup(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth);

struct blecon_bluetooth_gatt_server_t* blecon_nrf5_bluetooth_gatt_server_new(struct blecon_bluetooth_t* bluetooth, const uint8_t* characteristic_uuid);
struct blecon_bearer_t* blecon_nrf5_bluetooth_connection_get_gatt_server_bearer(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_gatt_server_t* gatts);

#ifdef __cplusplus
}
#endif
