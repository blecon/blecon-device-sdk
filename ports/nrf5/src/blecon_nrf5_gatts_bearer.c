/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blecon_nrf5/blecon_nrf5_gatts_bearer.h"
#include "blecon/blecon_error.h"
#include "blecon/blecon_util.h"
#include "blecon_nrf5_bluetooth_common.h"
#include "nrf_sdh_ble.h"
#include "ble_srv_common.h"

#include "stdint.h"
#include "string.h"

#define BLECON_NRF5_MAX_GATT_MTU (NRF_SDH_BLE_GATT_MAX_MTU_SIZE - 3) // 3 bytes for opcode and attribute handle

static struct blecon_buffer_t blecon_nrf5_gatts_bearer_alloc(struct blecon_bearer_t* bearer, size_t sz, void* user_data);
static size_t blecon_nrf5_gatts_bearer_mtu(struct blecon_bearer_t* bearer, void* user_data);
static void blecon_nrf5_gatts_bearer_send(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf, void* user_data);
static void blecon_nrf5_gatts_bearer_close(struct blecon_bearer_t* bearer, void* user_data);

static void blecon_nrf5_gatts_bearer_on_ble_evt(ble_evt_t const* p_ble_evt, void* p_context);
static void blecon_nrf5_gatts_bearer_close_callback(struct blecon_task_t* task, void* user_data);

static struct blecon_nrf5_bluetooth_t* _nrf5_bluetooth = NULL;
NRF_SDH_BLE_OBSERVER(blecon_nrf5_gatts_bearer_sdh_ble_obs, \
                     2 /* prio */,                 \
                     blecon_nrf5_gatts_bearer_on_ble_evt, NULL /* context not used */);

void blecon_nrf5_bluetooth_gatts_init(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth) {
    memset(&nrf5_bluetooth->gatts.bearers, 0, sizeof(nrf5_bluetooth->gatts.bearers));
    nrf5_bluetooth->gatts.bearers_count = 0;
    nrf5_bluetooth->gatts.service_handle = 0;
    nrf5_bluetooth->gatts.base_service_characteristics_uuid = 0;
    nrf5_bluetooth->gatts.connection_handle = BLE_CONN_HANDLE_INVALID;
    nrf5_bluetooth->gatts.att_mtu = BLE_GATT_ATT_MTU_DEFAULT;

    // Save pointer to bluetooth instance
    _nrf5_bluetooth = nrf5_bluetooth;
}

void blecon_nrf5_bluetooth_gatts_setup(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth) {
    // Register base UUID for GATT characteristics
    ble_uuid128_t base_uuid = { .uuid128 = { BLECON_UUID_ENDIANNESS_SWAP(BLECON_BASE_GATT_SERVICE_UUID) } };
    ret_code_t err_code = sd_ble_uuid_vs_add(&base_uuid, &nrf5_bluetooth->gatts.base_service_characteristics_uuid);
    blecon_assert(err_code == NRF_SUCCESS);

    // Add Blecon service
    ble_uuid_t blecon_service_uuid = {0};
    BLE_UUID_BLE_ASSIGN(blecon_service_uuid, BLECON_BLUETOOH_SERVICE_16UUID_VAL);
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &blecon_service_uuid, &nrf5_bluetooth->gatts.service_handle);
    blecon_assert(err_code == NRF_SUCCESS);
}

struct blecon_bluetooth_gatts_bearer_t* blecon_nrf5_bluetooth_gatts_bearer_new(struct blecon_bluetooth_t* bluetooth, const uint8_t* characteristic_uuid) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*)bluetooth;
    struct blecon_nrf5_gatts_t* nrf5_gatts = &nrf5_bluetooth->gatts;

    blecon_assert(nrf5_gatts->bearers_count < BLECON_NRF5_MAX_GATTS_BEARERS);
    struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = &nrf5_gatts->bearers[nrf5_gatts->bearers_count];

    const static struct blecon_bearer_fn_t bearer_fn = {
        .alloc = blecon_nrf5_gatts_bearer_alloc,
        .mtu = blecon_nrf5_gatts_bearer_mtu,
        .send = blecon_nrf5_gatts_bearer_send,
        .close = blecon_nrf5_gatts_bearer_close
    };

    blecon_bearer_set_functions(&nrf5_gatts_bearer->bearer, &bearer_fn, nrf5_gatts_bearer);

    // Add characteristic
    ble_add_char_params_t add_char_params = {0};
    add_char_params.uuid = nrf5_gatts->bearers_count & 0xffffu;
    add_char_params.uuid_type = nrf5_gatts->base_service_characteristics_uuid;
    add_char_params.max_len = BLECON_NRF5_MAX_GATT_MTU;
    add_char_params.is_var_len = true;
    add_char_params.char_props.write_wo_resp = true;
    add_char_params.char_props.notify = true;
    add_char_params.write_access = SEC_OPEN;
    add_char_params.cccd_write_access = SEC_OPEN;
    // add_char_params.is_value_user = true;

    ret_code_t err_code = characteristic_add(nrf5_gatts->service_handle, &add_char_params, &nrf5_gatts_bearer->handles);
    blecon_assert(err_code == NRF_SUCCESS);

    nrf5_gatts_bearer->connected = false;
    nrf5_gatts_bearer->pending_notifications_count = 0;
    blecon_scheduler_task_init(&nrf5_gatts_bearer->close_task);

    nrf5_gatts->bearers_count++;

    return &nrf5_gatts_bearer->gatts_bearer;
}

struct blecon_bearer_t* blecon_nrf5_bluetooth_gatts_bearer_as_bearer(struct blecon_bluetooth_gatts_bearer_t* gatts_bearer) {
    struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = (struct blecon_nrf5_gatts_bearer_t*)gatts_bearer;
    return &nrf5_gatts_bearer->bearer;
}

void blecon_nrf5_bluetooth_gatts_bearer_free(struct blecon_bluetooth_gatts_bearer_t* gatts_bearer) {
    blecon_fatal_error(); // Dynamic creation of GATTS bearers not supported on this platform
}

struct blecon_buffer_t blecon_nrf5_gatts_bearer_alloc(struct blecon_bearer_t* bearer, size_t sz, void* user_data) {
    if(sz > blecon_nrf5_gatts_bearer_mtu(bearer, user_data)) {
        blecon_fatal_error();
    }
    return blecon_buffer_alloc(sz);
}

size_t blecon_nrf5_gatts_bearer_mtu(struct blecon_bearer_t* bearer, void* user_data) {
    struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = (struct blecon_nrf5_gatts_bearer_t*)user_data;
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*)(nrf5_gatts_bearer->gatts_bearer.bluetooth);
    return nrf5_bluetooth->gatts.att_mtu - 3 /* 3 bytes for opcode and attribute handle */;
}

void blecon_nrf5_gatts_bearer_send(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf, void* user_data) {
    struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = (struct blecon_nrf5_gatts_bearer_t*)user_data;
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*)(nrf5_gatts_bearer->gatts_bearer.bluetooth);

    blecon_assert(nrf5_gatts_bearer->connected);

    // Increment pending notifications count
    nrf5_gatts_bearer->pending_notifications_count++;

    ble_gatts_hvx_params_t hvx_params = {0};
    uint16_t hvx_len = buf.sz;
    hvx_params.handle = nrf5_gatts_bearer->handles.value_handle;
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = 0;
    hvx_params.p_len = &hvx_len;
    hvx_params.p_data = buf.data;

    ret_code_t err_code = sd_ble_gatts_hvx(nrf5_bluetooth->gatts.connection_handle, &hvx_params);
    if( ((err_code == NRF_SUCCESS) && (hvx_len != buf.sz))
        || (err_code == NRF_ERROR_RESOURCES) ) {
        blecon_fatal_error();
    } else {
        blecon_assert(err_code == NRF_SUCCESS);
    }
    blecon_buffer_free(buf);
}

void blecon_nrf5_gatts_bearer_close(struct blecon_bearer_t* bearer, void* user_data) {
    struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = (struct blecon_nrf5_gatts_bearer_t*)user_data;
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*)(nrf5_gatts_bearer->gatts_bearer.bluetooth);

    if(!nrf5_gatts_bearer->connected) {
        return;
    }

    blecon_scheduler_queue_task(&nrf5_gatts_bearer->close_task, nrf5_bluetooth->bluetooth.scheduler, blecon_nrf5_gatts_bearer_close_callback, nrf5_gatts_bearer);
}

void blecon_nrf5_gatts_bearer_on_ble_evt(ble_evt_t const* p_ble_evt, void* p_context) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = _nrf5_bluetooth;
    struct blecon_nrf5_gatts_t* nrf5_gatts = &nrf5_bluetooth->gatts;

    // GAP events
    switch(p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED: {
            blecon_assert(nrf5_gatts->connection_handle == BLE_CONN_HANDLE_INVALID);

            // Save connection handle
            nrf5_gatts->connection_handle = p_ble_evt->evt.gap_evt.conn_handle;

            // Reset ATT MTU
            nrf5_gatts->att_mtu = BLE_GATT_ATT_MTU_DEFAULT;
            return;
        }
        case BLE_GAP_EVT_DISCONNECTED: {
            // Reset connection handle
            nrf5_gatts->connection_handle = BLE_CONN_HANDLE_INVALID;

            for(struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = nrf5_gatts->bearers; nrf5_gatts_bearer < nrf5_gatts->bearers + nrf5_gatts->bearers_count; nrf5_gatts_bearer++) {
                if(nrf5_gatts_bearer->connected) {
                    nrf5_gatts_bearer->connected = false;
                    blecon_bearer_on_closed(&nrf5_gatts_bearer->bearer);
                }
            }
            return;
        }
    }

    blecon_assert(nrf5_gatts->connection_handle == p_ble_evt->evt.gatts_evt.conn_handle);
    
    switch (p_ble_evt->header.evt_id) {
        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST: {
            // Negotiate ATT MTU
            uint16_t att_mtu = p_ble_evt->evt.gatts_evt.params.exchange_mtu_request.client_rx_mtu;
            if(att_mtu < BLE_GATT_ATT_MTU_DEFAULT) {
                att_mtu = BLE_GATT_ATT_MTU_DEFAULT;
            }
            if(att_mtu > NRF_SDH_BLE_GATT_MAX_MTU_SIZE) {
                att_mtu = NRF_SDH_BLE_GATT_MAX_MTU_SIZE;
            } 

            // Update ATT MTU
            nrf5_gatts->att_mtu = att_mtu;

            // Respond to ATT MTU request
            ret_code_t err_code = sd_ble_gatts_exchange_mtu_reply(p_ble_evt->evt.gatts_evt.conn_handle, att_mtu);
            blecon_assert(err_code == NRF_SUCCESS);
            break;
        }
        case BLE_GATTS_EVT_WRITE: {
            for(struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = nrf5_gatts->bearers; nrf5_gatts_bearer < nrf5_gatts->bearers + nrf5_gatts->bearers_count; nrf5_gatts_bearer++) {
                if(p_ble_evt->evt.gatts_evt.params.write.handle == nrf5_gatts_bearer->handles.cccd_handle) {
                    bool now_connected = ble_srv_is_notification_enabled(p_ble_evt->evt.gatts_evt.params.write.data);
                    if(now_connected != nrf5_gatts_bearer->connected) {
                        nrf5_gatts_bearer->connected = now_connected;
                        if(nrf5_gatts_bearer->connected) {
                            // Reset pending notifications count
                            nrf5_gatts_bearer->pending_notifications_count = 0;

                            blecon_bearer_on_open(&nrf5_gatts_bearer->bearer);
                        } else {
                            blecon_bearer_on_closed(&nrf5_gatts_bearer->bearer);
                        }
                    }
                    return;
                } else if(p_ble_evt->evt.gatts_evt.params.write.handle == nrf5_gatts_bearer->handles.value_handle) {
                    struct blecon_buffer_t buf = blecon_buffer_alloc(p_ble_evt->evt.gatts_evt.params.write.len);
                    memcpy(buf.data, p_ble_evt->evt.gatts_evt.params.write.data, buf.sz);
                    blecon_bearer_on_received(&nrf5_gatts_bearer->bearer, buf);
                    return;
                }
            }
            break;
        }
        case BLE_GATTS_EVT_HVN_TX_COMPLETE: {
            for(struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = nrf5_gatts->bearers; nrf5_gatts_bearer < nrf5_gatts->bearers + nrf5_gatts->bearers_count; nrf5_gatts_bearer++) {
                size_t hvn_tx_complete = p_ble_evt->evt.gatts_evt.params.hvn_tx_complete.count;
                while( (hvn_tx_complete > 0) && (nrf5_gatts_bearer->pending_notifications_count) ) {
                    hvn_tx_complete--;
                    nrf5_gatts_bearer->pending_notifications_count--;
                    blecon_bearer_on_sent(&nrf5_gatts_bearer->bearer);
                }
            }
            break;
        }
        default:
            break;
    }
}

void blecon_nrf5_gatts_bearer_close_callback(struct blecon_task_t* task, void* user_data) {
    struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = (struct blecon_nrf5_gatts_bearer_t*)user_data;

    if(!nrf5_gatts_bearer->connected) {
        return;
    }

    nrf5_gatts_bearer->connected = false;
    blecon_bearer_on_closed(&nrf5_gatts_bearer->bearer);
}
