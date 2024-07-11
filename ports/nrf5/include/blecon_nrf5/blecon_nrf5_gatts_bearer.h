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
#include "stddef.h"
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"

#define BLECON_NRF5_MAX_GATTS_BEARERS 2

struct blecon_nrf5_bluetooth_t;

struct blecon_nrf5_gatts_bearer_t {
    struct blecon_bluetooth_gatts_bearer_t gatts_bearer;
    struct blecon_bearer_t bearer;
    ble_gatts_char_handles_t handles;
    bool connected;
    size_t pending_notifications_count;
    struct blecon_task_t close_task;
};

struct blecon_nrf5_gatts_t {
    struct blecon_nrf5_gatts_bearer_t bearers[BLECON_NRF5_MAX_GATTS_BEARERS];
    size_t bearers_count;
    uint16_t service_handle;
    uint8_t base_service_characteristics_uuid;
    uint16_t connection_handle;
    size_t att_mtu;
};

void blecon_nrf5_bluetooth_gatts_init(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth);
void blecon_nrf5_bluetooth_gatts_setup(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth);

struct blecon_bluetooth_gatts_bearer_t* blecon_nrf5_bluetooth_gatts_bearer_new(struct blecon_bluetooth_t* bluetooth, const uint8_t* characteristic_uuid);
struct blecon_bearer_t* blecon_nrf5_bluetooth_gatts_bearer_as_bearer(struct blecon_bluetooth_gatts_bearer_t* gatts_bearer);
void blecon_nrf5_bluetooth_gatts_bearer_free(struct blecon_bluetooth_gatts_bearer_t* gatts_bearer);

#ifdef __cplusplus
}
#endif
