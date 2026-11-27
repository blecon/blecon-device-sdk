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

// A held notification refused with NRF_ERROR_INVALID_STATE is offered again after this, and again until the refusal ends:
// the connection interval the port prefers (MIN_CONN_INTERVAL, blecon_nrf5_bluetooth.c), the central setting the link's
#define BLECON_NRF5_GATTS_REFUSED_RETRY_MS 15

static struct blecon_buffer_t blecon_nrf5_gatts_bearer_alloc(struct blecon_bearer_t* bearer, size_t sz, void* user_data);
static size_t blecon_nrf5_gatts_bearer_mtu(struct blecon_bearer_t* bearer, void* user_data);
static void blecon_nrf5_gatts_bearer_send(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf, void* user_data);
static void blecon_nrf5_gatts_bearer_close(struct blecon_bearer_t* bearer, void* user_data);

static void blecon_nrf5_gatts_bearer_submit(struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer);
static void blecon_nrf5_gatts_submit(struct blecon_nrf5_gatts_t* nrf5_gatts);
static void blecon_nrf5_gatts_bearer_set_closed(struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer);
static void blecon_nrf5_gatts_in_flight_push(struct blecon_nrf5_gatts_t* nrf5_gatts, struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer);
static struct blecon_nrf5_gatts_bearer_t* blecon_nrf5_gatts_in_flight_pop(struct blecon_nrf5_gatts_t* nrf5_gatts);

static void blecon_nrf5_gatts_bearer_on_ble_evt(ble_evt_t const* p_ble_evt, void* p_context);
static void blecon_nrf5_gatts_bearer_close_callback(struct blecon_task_t* task, void* user_data);
static void blecon_nrf5_gatts_retry_timeout_callback(struct blecon_task_t* task, void* user_data);

static struct blecon_nrf5_bluetooth_t* _nrf5_bluetooth = NULL;
NRF_SDH_BLE_OBSERVER(blecon_nrf5_gatts_bearer_sdh_ble_obs, 2 /* prio */, blecon_nrf5_gatts_bearer_on_ble_evt, NULL /* context not used */);

void blecon_nrf5_bluetooth_gatt_server_init(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth) {
    memset(&nrf5_bluetooth->gatts.bearers, 0, sizeof(nrf5_bluetooth->gatts.bearers));
    nrf5_bluetooth->gatts.bearers_count = 0;
    nrf5_bluetooth->gatts.service_handle = 0;
    nrf5_bluetooth->gatts.base_service_characteristics_uuid = 0;
    // nrf5_bluetooth->gatts.connection_handle = BLE_CONN_HANDLE_INVALID;
    nrf5_bluetooth->gatts.att_mtu = BLE_GATT_ATT_MTU_DEFAULT;
    nrf5_bluetooth->gatts.in_flight_first = 0;
    nrf5_bluetooth->gatts.in_flight_count = 0;
    nrf5_bluetooth->gatts.submit_first = 0;
    blecon_timeout_init(&nrf5_bluetooth->gatts.retry_timeout, blecon_nrf5_gatts_retry_timeout_callback, nrf5_bluetooth);
    nrf5_bluetooth->gatts.retry_queued = false;

    // Save pointer to bluetooth instance
    _nrf5_bluetooth = nrf5_bluetooth;
}

void blecon_nrf5_bluetooth_gatt_server_setup(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth) {
    // Register base UUID for GATT characteristics
    ble_uuid128_t base_uuid = { .uuid128 = { BLECON_UUID_ENDIANNESS_SWAP(BLECON_BASE_GATT_SERVICE_UUID) } };
    ret_code_t err_code = sd_ble_uuid_vs_add(&base_uuid, &nrf5_bluetooth->gatts.base_service_characteristics_uuid);
    blecon_assert(err_code == NRF_SUCCESS);

    // Add Blecon service
    ble_uuid_t blecon_service_uuid = { 0 };
    BLE_UUID_BLE_ASSIGN(blecon_service_uuid, BLECON_BLUETOOTH_SERVICE_16UUID_VAL);
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &blecon_service_uuid, &nrf5_bluetooth->gatts.service_handle);
    blecon_assert(err_code == NRF_SUCCESS);
}

struct blecon_bluetooth_gatt_server_t* blecon_nrf5_bluetooth_gatt_server_new(struct blecon_bluetooth_t* bluetooth, const uint8_t* characteristic_uuid) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*) bluetooth;
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
    ble_add_char_params_t add_char_params = { 0 };
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
    blecon_buffer_queue_init(&nrf5_gatts_bearer->held);
    blecon_task_init(&nrf5_gatts_bearer->close_task, blecon_nrf5_gatts_bearer_close_callback, nrf5_gatts_bearer);

    nrf5_gatts->bearers_count++;

    return &nrf5_gatts_bearer->gatt_server;
}

struct blecon_bearer_t* blecon_nrf5_bluetooth_connection_get_gatt_server_bearer(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_gatt_server_t* gatts) {
    struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = (struct blecon_nrf5_gatts_bearer_t*) gatts;
    return &nrf5_gatts_bearer->bearer;
}

struct blecon_buffer_t blecon_nrf5_gatts_bearer_alloc(struct blecon_bearer_t* bearer, size_t sz, void* user_data) {
    if( sz > blecon_nrf5_gatts_bearer_mtu(bearer, user_data) ) {
        blecon_fatal_error();
    }
    return blecon_buffer_queue_alloc(sz); // The send holds it in the bearer's queue
}

size_t blecon_nrf5_gatts_bearer_mtu(struct blecon_bearer_t* bearer, void* user_data) {
    struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = (struct blecon_nrf5_gatts_bearer_t*) user_data;
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*) (nrf5_gatts_bearer->gatt_server.bluetooth);
    return nrf5_bluetooth->gatts.att_mtu - 3 /* 3 bytes for opcode and attribute handle */;
}

void blecon_nrf5_gatts_bearer_send(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf, void* user_data) {
    struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = (struct blecon_nrf5_gatts_bearer_t*) user_data;

    blecon_assert(nrf5_gatts_bearer->connected);

    // Behind those the bearer already holds, so the bearer's notifications reach the SoftDevice in order
    blecon_buffer_queue_push(&nrf5_gatts_bearer->held, buf);
    blecon_nrf5_gatts_bearer_submit(nrf5_gatts_bearer);
}

void blecon_nrf5_gatts_bearer_close(struct blecon_bearer_t* bearer, void* user_data) {
    struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = (struct blecon_nrf5_gatts_bearer_t*) user_data;
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*) (nrf5_gatts_bearer->gatt_server.bluetooth);

    if( !nrf5_gatts_bearer->connected ) {
        return;
    }

    // A second close before the task has run leaves one queued
    blecon_task_cancel(&nrf5_gatts_bearer->close_task);
    blecon_scheduler_queue_task(nrf5_bluetooth->bluetooth.scheduler, &nrf5_gatts_bearer->close_task);
}

// Offers the bearer's held notifications to the SoftDevice, oldest first, until it refuses one or the in-flight record is
// full. Raises nothing
void blecon_nrf5_gatts_bearer_submit(struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*) (nrf5_gatts_bearer->gatt_server.bluetooth);
    struct blecon_nrf5_gatts_t* nrf5_gatts = &nrf5_bluetooth->gatts;

    while( !blecon_buffer_queue_is_empty(&nrf5_gatts_bearer->held) ) {
        if( nrf5_gatts->in_flight_count == BLECON_NRF5_GATTS_IN_FLIGHT_MAX ) {
            // The next BLE_GATTS_EVT_HVN_TX_COMPLETE makes room
            return;
        }

        struct blecon_buffer_t buf = blecon_buffer_queue_peek(&nrf5_gatts_bearer->held);

        ble_gatts_hvx_params_t hvx_params = { 0 };
        uint16_t hvx_len = buf.sz;
        hvx_params.handle = nrf5_gatts_bearer->handles.value_handle;
        hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
        hvx_params.offset = 0;
        hvx_params.p_len = &hvx_len;
        hvx_params.p_data = buf.data;

        ret_code_t err_code = sd_ble_gatts_hvx(nrf5_bluetooth->connection.handle, &hvx_params);
        if( err_code == NRF_ERROR_RESOURCES ) {
            // The link's notification queue is full, and of the port's notifications alone (the SoftDevice guarantees it
            // one, BLE_GATTS_HVN_TX_QUEUE_SIZE_DEFAULT, the port configuring no more): one in flight at least, whose
            // BLE_GATTS_EVT_HVN_TX_COMPLETE offers this one again
            blecon_assert(nrf5_gatts->in_flight_count > 0);
            return;
        }
        if( (err_code == BLE_ERROR_INVALID_CONN_HANDLE) || (err_code == NRF_ERROR_TIMEOUT) ) {
            // The SoftDevice has ended the link, or a GATT procedure has timed out on it (BLE_GATTS_EVT_TIMEOUT or
            // BLE_GATTC_EVT_TIMEOUT queued, which the port answers with a disconnection): the link's
            // BLE_GAP_EVT_DISCONNECTED closes the bearer and frees what it holds
            return;
        }
        if( err_code == NRF_ERROR_INVALID_STATE ) {
            // One of three causes, which the port cannot tell apart here: the SoftDevice has ended the link (its
            // BLE_GAP_EVT_DISCONNECTED queued), the peer has turned notifications off (its CCCD write's BLE_GATTS_EVT_WRITE
            // queued; the SoftDevice answers that write itself, so it reaches the port before the peer's next request),
            // or an ATT_MTU exchange is ongoing on a live link (the peer's BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST queued, or
            // the port's reply not yet sent on air). The first two close the bearer, which frees what it holds; the third
            // raises no event at its end, so the held notifications are offered again after a connection interval
            if( !nrf5_gatts->retry_queued ) {
                nrf5_gatts->retry_queued = true;
                blecon_scheduler_queue_timeout(nrf5_bluetooth->bluetooth.scheduler, &nrf5_gatts->retry_timeout, BLECON_NRF5_GATTS_REFUSED_RETRY_MS);
            }
            return;
        }
        // Every other code is fatal: among them BLE_ERROR_GATTS_SYS_ATTR_MISSING, which cannot be once the peer's CCCD
        // write has opened the bearer (assumed, as at BLE_GATTS_EVT_SYS_ATTR_MISSING)
        blecon_assert(err_code == NRF_SUCCESS);
        blecon_assert(hvx_len == buf.sz);

        // Copied by the SoftDevice
        blecon_buffer_free(blecon_buffer_queue_pop(&nrf5_gatts_bearer->held));
        blecon_nrf5_gatts_in_flight_push(nrf5_gatts, nrf5_gatts_bearer);
    }
}

// Offers every open bearer's held notifications, from a bearer one on from the last offer's first: with room for one at a
// time, the bearers take turns
void blecon_nrf5_gatts_submit(struct blecon_nrf5_gatts_t* nrf5_gatts) {
    if( nrf5_gatts->bearers_count == 0 ) {
        return;
    }

    for( size_t idx = 0; idx < nrf5_gatts->bearers_count; idx++ ) {
        struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = &nrf5_gatts->bearers[(nrf5_gatts->submit_first + idx) % nrf5_gatts->bearers_count];
        if( nrf5_gatts_bearer->connected ) {
            blecon_nrf5_gatts_bearer_submit(nrf5_gatts_bearer);
        }
    }
    nrf5_gatts->submit_first = (nrf5_gatts->submit_first + 1) % nrf5_gatts->bearers_count;
}

// Closes an open bearer: its user's close, if queued, is dropped (it would close the bearer's next opening), what it holds
// is freed, its notifications in flight are no longer its own (they raise no on_sent, should it open again before they are
// sent), and then on_closed is raised
void blecon_nrf5_gatts_bearer_set_closed(struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*) (nrf5_gatts_bearer->gatt_server.bluetooth);
    struct blecon_nrf5_gatts_t* nrf5_gatts = &nrf5_bluetooth->gatts;

    blecon_assert(nrf5_gatts_bearer->connected);
    nrf5_gatts_bearer->connected = false;
    blecon_task_cancel(&nrf5_gatts_bearer->close_task);
    blecon_buffer_queue_clear(&nrf5_gatts_bearer->held);
    for( size_t idx = 0; idx < nrf5_gatts->in_flight_count; idx++ ) {
        size_t slot = (nrf5_gatts->in_flight_first + idx) % BLECON_NRF5_GATTS_IN_FLIGHT_MAX;
        if( nrf5_gatts->in_flight[slot] == nrf5_gatts_bearer ) {
            nrf5_gatts->in_flight[slot] = NULL;
        }
    }

    blecon_bearer_on_closed(&nrf5_gatts_bearer->bearer);
}

void blecon_nrf5_gatts_in_flight_push(struct blecon_nrf5_gatts_t* nrf5_gatts, struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer) {
    blecon_assert(nrf5_gatts->in_flight_count < BLECON_NRF5_GATTS_IN_FLIGHT_MAX);
    size_t slot = (nrf5_gatts->in_flight_first + nrf5_gatts->in_flight_count) % BLECON_NRF5_GATTS_IN_FLIGHT_MAX;
    nrf5_gatts->in_flight[slot] = nrf5_gatts_bearer;
    nrf5_gatts->in_flight_count++;
}

struct blecon_nrf5_gatts_bearer_t* blecon_nrf5_gatts_in_flight_pop(struct blecon_nrf5_gatts_t* nrf5_gatts) {
    blecon_assert(nrf5_gatts->in_flight_count > 0);
    struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = nrf5_gatts->in_flight[nrf5_gatts->in_flight_first];
    nrf5_gatts->in_flight_first = (nrf5_gatts->in_flight_first + 1) % BLECON_NRF5_GATTS_IN_FLIGHT_MAX;
    nrf5_gatts->in_flight_count--;
    return nrf5_gatts_bearer;
}

void blecon_nrf5_gatts_bearer_on_ble_evt(ble_evt_t const* p_ble_evt, void* p_context) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = _nrf5_bluetooth;
    struct blecon_nrf5_gatts_t* nrf5_gatts = &nrf5_bluetooth->gatts;

    // GAP events
    switch( p_ble_evt->header.evt_id ) {
        case BLE_GAP_EVT_CONNECTED: {
            // Reset ATT MTU
            nrf5_gatts->att_mtu = BLE_GATT_ATT_MTU_DEFAULT;
            return;
        }
        case BLE_GAP_EVT_DISCONNECTED: {
            // The SoftDevice drops the notifications it holds, and reports none of them sent
            nrf5_gatts->in_flight_first = 0;
            nrf5_gatts->in_flight_count = 0;
            if( nrf5_gatts->retry_queued ) {
                blecon_timeout_cancel(&nrf5_gatts->retry_timeout);
                nrf5_gatts->retry_queued = false;
            }
            for( struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = nrf5_gatts->bearers; nrf5_gatts_bearer < nrf5_gatts->bearers + nrf5_gatts->bearers_count; nrf5_gatts_bearer++ ) {
                if( nrf5_gatts_bearer->connected ) {
                    blecon_nrf5_gatts_bearer_set_closed(nrf5_gatts_bearer);
                }
            }
            return;
        }
    }

    // GATTS events only past this point: the observer sees every BLE event, and a GAP event of no connection (an
    // advertising set's end, a scan report), which names BLE_CONN_HANDLE_INVALID, may come during a connection
    if( (p_ble_evt->header.evt_id < BLE_GATTS_EVT_BASE) || (p_ble_evt->header.evt_id > BLE_GATTS_EVT_LAST) ) {
        return;
    }

    blecon_assert(nrf5_bluetooth->connection.handle == p_ble_evt->evt.gatts_evt.conn_handle);

    switch( p_ble_evt->header.evt_id ) {
        case BLE_GATTS_EVT_SYS_ATTR_MISSING: {
            // A peer's access to a system attribute (a CCCD's value) waits for the connection's system attributes, which
            // the port, having no bonding, has none of: the SoftDevice's defaults (NULL), as Nordic's examples answer.
            // Those defaults turn every CCCD off, so the event must come before any bearer opens: a bearer opens at its
            // CCCD's write, assumed to leave the connection with system attributes (no header says so)
            for( struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = nrf5_gatts->bearers; nrf5_gatts_bearer < nrf5_gatts->bearers + nrf5_gatts->bearers_count; nrf5_gatts_bearer++ ) {
                blecon_assert(!nrf5_gatts_bearer->connected);
            }
            ret_code_t err_code = sd_ble_gatts_sys_attr_set(p_ble_evt->evt.gatts_evt.conn_handle, NULL, 0, 0);
            // BLE_ERROR_INVALID_CONN_HANDLE or NRF_ERROR_INVALID_STATE (Invalid Connection State): the SoftDevice has ended
            // the link since raising this event, and its BLE_GAP_EVT_DISCONNECTED follows
            blecon_assert((err_code == NRF_SUCCESS) || (err_code == BLE_ERROR_INVALID_CONN_HANDLE) || (err_code == NRF_ERROR_INVALID_STATE));
            break;
        }
        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST: {
            // Negotiate ATT MTU
            uint16_t att_mtu = p_ble_evt->evt.gatts_evt.params.exchange_mtu_request.client_rx_mtu;
            if( att_mtu < BLE_GATT_ATT_MTU_DEFAULT ) {
                att_mtu = BLE_GATT_ATT_MTU_DEFAULT;
            }
            if( att_mtu > NRF_SDH_BLE_GATT_MAX_MTU_SIZE ) {
                att_mtu = NRF_SDH_BLE_GATT_MAX_MTU_SIZE;
            }

            // Respond to ATT MTU request
            ret_code_t err_code = sd_ble_gatts_exchange_mtu_reply(p_ble_evt->evt.gatts_evt.conn_handle, att_mtu);
            if( (err_code == BLE_ERROR_INVALID_CONN_HANDLE) || (err_code == NRF_ERROR_INVALID_STATE) ) {
                // The SoftDevice has ended the link since raising this event (NRF_ERROR_INVALID_STATE's other cause, no
                // request pending, cannot be while its request's event is handled), and its BLE_GAP_EVT_DISCONNECTED,
                // which closes the bearers, follows: the MTU stays
                break;
            }
            blecon_assert(err_code == NRF_SUCCESS);

            // Update ATT MTU
            nrf5_gatts->att_mtu = att_mtu;
            break;
        }
        case BLE_GATTS_EVT_WRITE: {
            for( struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = nrf5_gatts->bearers; nrf5_gatts_bearer < nrf5_gatts->bearers + nrf5_gatts->bearers_count; nrf5_gatts_bearer++ ) {
                if( p_ble_evt->evt.gatts_evt.params.write.handle == nrf5_gatts_bearer->handles.cccd_handle ) {
                    bool now_connected = ble_srv_is_notification_enabled(p_ble_evt->evt.gatts_evt.params.write.data);
                    if( now_connected != nrf5_gatts_bearer->connected ) {
                        if( now_connected ) {
                            nrf5_gatts_bearer->connected = true;
                            blecon_bearer_on_open(&nrf5_gatts_bearer->bearer);
                        } else {
                            blecon_nrf5_gatts_bearer_set_closed(nrf5_gatts_bearer);
                        }
                    }
                    return;
                } else if( p_ble_evt->evt.gatts_evt.params.write.handle == nrf5_gatts_bearer->handles.value_handle ) {
                    struct blecon_buffer_t buf = blecon_buffer_alloc(p_ble_evt->evt.gatts_evt.params.write.len);
                    memcpy(buf.data, p_ble_evt->evt.gatts_evt.params.write.data, buf.sz);
                    blecon_bearer_on_received(&nrf5_gatts_bearer->bearer, buf);
                    return;
                }
            }
            break;
        }
        case BLE_GATTS_EVT_HVN_TX_COMPLETE: {
            // The SoftDevice sends the link's notifications in the order it took them, and all of them are the port's: the
            // count is of the oldest in flight
            size_t count = p_ble_evt->evt.gatts_evt.params.hvn_tx_complete.count;
            blecon_assert(count <= nrf5_gatts->in_flight_count);
            struct blecon_nrf5_gatts_bearer_t* sent[BLECON_NRF5_GATTS_IN_FLIGHT_MAX];
            for( size_t idx = 0; idx < count; idx++ ) {
                sent[idx] = blecon_nrf5_gatts_in_flight_pop(nrf5_gatts);
            }

            // The room made goes to the held notifications first, then each sent notification's bearer, if it has not
            // closed since, is told: its on_sent may send again, behind what it holds
            blecon_nrf5_gatts_submit(nrf5_gatts);
            for( size_t idx = 0; idx < count; idx++ ) {
                if( sent[idx] != NULL ) {
                    blecon_bearer_on_sent(&sent[idx]->bearer);
                }
            }
            break;
        }
        default:
            break;
    }
}

void blecon_nrf5_gatts_bearer_close_callback(struct blecon_task_t* task, void* user_data) {
    struct blecon_nrf5_gatts_bearer_t* nrf5_gatts_bearer = (struct blecon_nrf5_gatts_bearer_t*) user_data;

    // Queued only on an open bearer, and cancelled by its every close
    blecon_nrf5_gatts_bearer_set_closed(nrf5_gatts_bearer);
}

void blecon_nrf5_gatts_retry_timeout_callback(struct blecon_task_t* task, void* user_data) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*) user_data;

    nrf5_bluetooth->gatts.retry_queued = false;
    blecon_nrf5_gatts_submit(&nrf5_bluetooth->gatts);
}
