/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stdlib.h"
#include "string.h"

#include "blecon_zephyr_gatts_bearer.h"
#include "blecon/blecon_defs.h"
#include "blecon/blecon_memory.h"
#include "blecon/blecon_buffer.h"
#include "blecon/blecon_buffer_queue.h"
#include "blecon/blecon_error.h"
#include "blecon/blecon_util.h"
#include "blecon/port/blecon_event_loop.h"

#include "blecon_zephyr_bluetooth_common.h"

#include "zephyr/bluetooth/bluetooth.h"
#include "zephyr/bluetooth/conn.h"
#include "zephyr/bluetooth/gatt.h"

#define BT_ATT_ERR_APPLICATION_0 0x80 // First ATT error code reserved for application

static struct blecon_buffer_t blecon_zephyr_gatts_bearer_alloc(struct blecon_bearer_t* bearer, size_t sz, void* user_data);
static size_t blecon_zephyr_gatts_bearer_mtu(struct blecon_bearer_t* bearer, void* user_data);
static void blecon_zephyr_gatts_bearer_send(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf, void* user_data);
static void blecon_zephyr_gatts_bearer_close(struct blecon_bearer_t* bearer, void* user_data);

static ssize_t blecon_zephyr_gatts_bearer_gatt_write(struct bt_conn* conn,
    const struct bt_gatt_attr* attr, const void* buf, uint16_t len,
    uint16_t offset, uint8_t flags);
static void blecon_zephyr_gatts_bearer_gatt_cccd_changed(const struct bt_gatt_attr* attr, uint16_t value);
static ssize_t blecon_zephyr_gatts_bearer_gatt_cccd_write(struct bt_conn* conn, const struct bt_gatt_attr* attr, uint16_t value);
static void blecon_zephyr_gatts_bearer_gatt_notify_done(struct bt_conn* conn, void* user_data);
static void blecon_zephyr_gatts_bearer_close_callback(struct blecon_task_t* task, void* user_data);

static struct blecon_zephyr_bluetooth_t* _zephyr_bluetooth = NULL;

// By default, Zephyr supports statically definined GATT service,
// so we avoid having to enable dynamic GATT support to save 
// on ROM usage
#define BLECON_GATT_BEARER_CHARACTERISTIC(idx) \
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(BLECON_UUID_ENDIANNESS_SWAP(BLECON_GATT_CHARACTERISTIC_UUID(idx))), \
        BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_WRITE_WITHOUT_RESP, \
        BT_GATT_PERM_WRITE, \
        NULL, blecon_zephyr_gatts_bearer_gatt_write, \
        (void*)idx), \
    BT_GATT_CCC_MANAGED(((struct _bt_gatt_ccc[])                    \
    {BT_GATT_CCC_INITIALIZER(blecon_zephyr_gatts_bearer_gatt_cccd_changed, blecon_zephyr_gatts_bearer_gatt_cccd_write, NULL)}), BT_GATT_PERM_WRITE)

BT_GATT_SERVICE_DEFINE(blecon_gatt_service,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(BLECON_BLUETOOTH_SERVICE_16UUID_VAL)),
	BLECON_GATT_BEARER_CHARACTERISTIC(0),
    BLECON_GATT_BEARER_CHARACTERISTIC(1),
);

void blecon_zephyr_bluetooth_gatt_server_init(struct blecon_zephyr_bluetooth_t* zephyr_bluetooth) {
    struct blecon_zephyr_gatts_t* zephyr_gatts = &zephyr_bluetooth->gatts;
    memset(zephyr_gatts->bearers, 0, sizeof(zephyr_gatts->bearers));
    zephyr_gatts->bearers_count = 0;

    // Save global reference to bluetooth object
    _zephyr_bluetooth = zephyr_bluetooth;
}

struct blecon_bluetooth_gatt_server_t* blecon_zephyr_bluetooth_gatt_server_new(struct blecon_bluetooth_t* bluetooth, const uint8_t* characteristic_uuid) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = (struct blecon_zephyr_bluetooth_t*)bluetooth;
    struct blecon_zephyr_gatts_t* zephyr_gatts = &zephyr_bluetooth->gatts;

    blecon_assert(zephyr_gatts->bearers_count < BLECON_ZEPHYR_MAX_GATTS_BEARERS);
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = &zephyr_gatts->bearers[zephyr_gatts->bearers_count];

    zephyr_gatts_bearer->bluetooth = zephyr_bluetooth;

    const static struct blecon_bearer_fn_t bearer_fn = {
        .alloc = blecon_zephyr_gatts_bearer_alloc,
        .mtu = blecon_zephyr_gatts_bearer_mtu,
        .send = blecon_zephyr_gatts_bearer_send,
        .close = blecon_zephyr_gatts_bearer_close
    };

    blecon_bearer_set_functions(&zephyr_gatts_bearer->bearer, &bearer_fn, zephyr_gatts_bearer);

    zephyr_gatts_bearer->attr = &blecon_gatt_service.attrs[2 + zephyr_gatts->bearers_count * 3];
    zephyr_gatts_bearer->connected = false;
    blecon_task_init(&zephyr_gatts_bearer->close_task, blecon_zephyr_gatts_bearer_close_callback, zephyr_gatts_bearer);
    
    zephyr_gatts->bearers_count++;

    return &zephyr_gatts_bearer->gatt_server;
}

struct blecon_bearer_t* blecon_zephyr_bluetooth_connection_get_gatt_server_bearer(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_gatt_server_t* gatt_server) {
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = (struct blecon_zephyr_bluetooth_gatt_server_bearer_t*)gatt_server;
    return &zephyr_gatts_bearer->bearer;
}

struct blecon_buffer_t blecon_zephyr_gatts_bearer_alloc(struct blecon_bearer_t* bearer, size_t sz, void* user_data) {
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = (struct blecon_zephyr_bluetooth_gatt_server_bearer_t*)user_data;
    (void)zephyr_gatts_bearer;
    
    if(sz > blecon_zephyr_gatts_bearer_mtu(bearer, user_data)) {
        blecon_fatal_error();
    }
    return blecon_buffer_alloc(sz);
}

size_t blecon_zephyr_gatts_bearer_mtu(struct blecon_bearer_t* bearer, void* user_data) {
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = (struct blecon_zephyr_bluetooth_gatt_server_bearer_t*)user_data;
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = zephyr_gatts_bearer->bluetooth;

    return bt_gatt_get_mtu(zephyr_bluetooth->connection.conn) - 3 /* opcode and handle */;
}

void blecon_zephyr_gatts_bearer_send(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf, void* user_data) {
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = (struct blecon_zephyr_bluetooth_gatt_server_bearer_t*)user_data;
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = zephyr_gatts_bearer->bluetooth;

    if(!bt_gatt_is_subscribed(zephyr_bluetooth->connection.conn, zephyr_gatts_bearer->attr, BT_GATT_CCC_NOTIFY)) {
        // A failure here could mean the client has disconnected, but the disconnection event hasn't propagated yet
        // So only return here
        blecon_buffer_free(buf);
        return;
    }

    struct bt_gatt_notify_params params = {
        .attr = zephyr_gatts_bearer->attr,
        .data = buf.data,
        .len = buf.sz,
        .func = blecon_zephyr_gatts_bearer_gatt_notify_done,
        .user_data = zephyr_gatts_bearer
    };

    int ret = bt_gatt_notify_cb(zephyr_bluetooth->connection.conn, &params);
    if(ret) {
        blecon_fatal_error();
    }

    blecon_buffer_free(buf);
}

void blecon_zephyr_gatts_bearer_close(struct blecon_bearer_t* bearer, void* user_data) {
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = (struct blecon_zephyr_bluetooth_gatt_server_bearer_t*)user_data;
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = zephyr_gatts_bearer->bluetooth;

    if(!zephyr_gatts_bearer->connected) {
        return;
    }

    blecon_scheduler_queue_task(zephyr_bluetooth->bluetooth.scheduler, &zephyr_gatts_bearer->close_task);
}

ssize_t blecon_zephyr_gatts_bearer_gatt_write(struct bt_conn* conn,
    const struct bt_gatt_attr* attr, const void* buf, uint16_t len, uint16_t offset, uint8_t flags) 
{
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = _zephyr_bluetooth;
    size_t bearer_idx = (size_t) attr->user_data;
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = &zephyr_bluetooth->gatts.bearers[bearer_idx];

    blecon_event_loop_lock(zephyr_bluetooth->event_loop);

    // Ignore if not a Blecon connection
    if(conn != zephyr_bluetooth->connection.conn) {
        blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
        return len; // Accept the write, but ignore it
    }
    
    struct blecon_buffer_t bearer_buf = blecon_buffer_alloc(len);

    memcpy(bearer_buf.data, buf, len);

    blecon_bearer_on_received(&zephyr_gatts_bearer->bearer, bearer_buf);
    blecon_event_loop_unlock(zephyr_bluetooth->event_loop);

    return len;
}

void blecon_zephyr_gatts_bearer_gatt_cccd_changed(const struct bt_gatt_attr* attr, uint16_t value) {
    // Bit of a hack, but we know that the CCC descriptor follows
    // the characteristic's attribute
    // The CCC descriptor attribute's user data points to a structure
    // managed by Zephyr, so we can't use it to store our own data
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = _zephyr_bluetooth;
    const struct bt_gatt_attr* characteristic_attr = attr - 1;
    size_t bearer_idx = (size_t) characteristic_attr->user_data;
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = &zephyr_bluetooth->gatts.bearers[bearer_idx];

    bool now_connected = value == BT_GATT_CCC_NOTIFY;
    blecon_event_loop_lock(zephyr_bluetooth->event_loop);
    if(now_connected == zephyr_gatts_bearer->connected) {
        blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
        return;
    }
    zephyr_gatts_bearer->connected = now_connected;
    if(zephyr_gatts_bearer->connected) {
        blecon_bearer_on_open(&zephyr_gatts_bearer->bearer);
    } else {
        blecon_bearer_on_closed(&zephyr_gatts_bearer->bearer);
    }
    blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
}


ssize_t blecon_zephyr_gatts_bearer_gatt_cccd_write(struct bt_conn* conn, const struct bt_gatt_attr* attr, uint16_t value) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = _zephyr_bluetooth;

    // The Zephyr host stack performs the following operations when handling a write to the CCCD:
    // * Call write callback with the new value
    // * Abort if the write is not accepted
    // * Store the new value
    // * Call CCCD changed callback with the new value
    // Therefore, we only check if the write is made on the correct connection
    // If not, we reject it
    // The changed callback will be called with the new value, if the write is accepted
    // We don't handle the code which is in the changed callback here, as the new value CCCD hasn't been stored yet
    // Which means bt_gatt_is_subscribed() will return false at this stage, and blecon_zephyr_gatts_bearer_send() would assert

    // Ignore if not a Blecon connection
    if(conn != zephyr_bluetooth->connection.conn) {
        blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
        return BT_GATT_ERR(BT_ATT_ERR_APPLICATION_0); // Reject the write
    }

    blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
    return sizeof(value);
}


void blecon_zephyr_gatts_bearer_gatt_notify_done(struct bt_conn* conn, void* user_data) {
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = user_data;
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = zephyr_gatts_bearer->bluetooth;
    blecon_event_loop_lock(zephyr_bluetooth->event_loop);
    blecon_bearer_on_sent(&zephyr_gatts_bearer->bearer);
    blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
}

void blecon_zephyr_gatts_bearer_close_callback(struct blecon_task_t* task, void* user_data) {
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = user_data;
    if(!zephyr_gatts_bearer->connected) {
        return;
    }
    zephyr_gatts_bearer->connected = false;
    blecon_bearer_on_closed(&zephyr_gatts_bearer->bearer);
}
