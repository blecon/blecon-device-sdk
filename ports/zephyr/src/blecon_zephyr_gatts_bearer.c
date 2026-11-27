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

static ssize_t blecon_zephyr_gatts_bearer_gatt_write(struct bt_conn* conn, const struct bt_gatt_attr* attr, const void* buf, uint16_t len, uint16_t offset, uint8_t flags);
static void blecon_zephyr_gatts_bearer_gatt_cccd_changed(const struct bt_gatt_attr* attr, uint16_t value);
static ssize_t blecon_zephyr_gatts_bearer_gatt_cccd_write(struct bt_conn* conn, const struct bt_gatt_attr* attr, uint16_t value);
static ssize_t blecon_zephyr_gatts_bearer_gatt_cccd_refuse(struct bt_conn* conn, const struct bt_gatt_attr* attr, uint8_t att_err);
static void blecon_zephyr_gatts_bearer_gatt_notify_done(struct bt_conn* conn, void* user_data);
static void blecon_zephyr_gatts_bearer_close_callback(struct blecon_task_t* task, void* user_data);
static void blecon_zephyr_gatts_bearer_set_closed(struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer);

static struct blecon_zephyr_bluetooth_t* _zephyr_bluetooth = NULL;

// By default, Zephyr supports statically definined GATT service,
// so we avoid having to enable dynamic GATT support to save
// on ROM usage
#define BLECON_GATT_BEARER_CHARACTERISTIC(idx)                                                                                                                                                                                                           \
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(BLECON_UUID_ENDIANNESS_SWAP(BLECON_GATT_CHARACTERISTIC_UUID(idx))), BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_WRITE_WITHOUT_RESP, BT_GATT_PERM_WRITE, NULL, blecon_zephyr_gatts_bearer_gatt_write, (void*) idx), \
        BT_GATT_CCC_MANAGED(((struct bt_gatt_ccc_managed_user_data[]){ BT_GATT_CCC_MANAGED_USER_DATA_INIT(blecon_zephyr_gatts_bearer_gatt_cccd_changed, blecon_zephyr_gatts_bearer_gatt_cccd_write, NULL) }), BT_GATT_PERM_WRITE)

BT_GATT_SERVICE_DEFINE(blecon_gatt_service, BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(BLECON_BLUETOOTH_SERVICE_16UUID_VAL)), BLECON_GATT_BEARER_CHARACTERISTIC(0), BLECON_GATT_BEARER_CHARACTERISTIC(1), );

void blecon_zephyr_bluetooth_gatt_server_init(struct blecon_zephyr_bluetooth_t* zephyr_bluetooth) {
    struct blecon_zephyr_gatts_t* zephyr_gatts = &zephyr_bluetooth->gatts;
    memset(zephyr_gatts->bearers, 0, sizeof(zephyr_gatts->bearers));
    zephyr_gatts->bearers_count = 0;

    // Save global reference to bluetooth object
    _zephyr_bluetooth = zephyr_bluetooth;
}

struct blecon_bluetooth_gatt_server_t* blecon_zephyr_bluetooth_gatt_server_new(struct blecon_bluetooth_t* bluetooth, const uint8_t* characteristic_uuid) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = (struct blecon_zephyr_bluetooth_t*) bluetooth;
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
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = (struct blecon_zephyr_bluetooth_gatt_server_bearer_t*) gatt_server;
    return &zephyr_gatts_bearer->bearer;
}

struct blecon_buffer_t blecon_zephyr_gatts_bearer_alloc(struct blecon_bearer_t* bearer, size_t sz, void* user_data) {
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = (struct blecon_zephyr_bluetooth_gatt_server_bearer_t*) user_data;
    (void) zephyr_gatts_bearer;

    if( sz > blecon_zephyr_gatts_bearer_mtu(bearer, user_data) ) {
        blecon_fatal_error();
    }
    return blecon_buffer_alloc(sz);
}

size_t blecon_zephyr_gatts_bearer_mtu(struct blecon_bearer_t* bearer, void* user_data) {
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = (struct blecon_zephyr_bluetooth_gatt_server_bearer_t*) user_data;
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = zephyr_gatts_bearer->bluetooth;

    // The connection's record of its link's ATT MTU, not bt_gatt_get_mtu: Zephyr reads 0 from the moment the link leaves
    // its connected state (connection_disconnect's request, or the controller's Disconnection Complete processed) while
    // the bearer stays open until the CCC's clearing, so a user still sending would read 0 - 3, wrapped
    return zephyr_bluetooth->connection.att_mtu - 3 /* opcode and handle */;
}

void blecon_zephyr_gatts_bearer_send(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf, void* user_data) {
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = (struct blecon_zephyr_bluetooth_gatt_server_bearer_t*) user_data;
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = zephyr_gatts_bearer->bluetooth;
    struct bt_conn* conn = zephyr_bluetooth->connection.conn;

    // A bearer's user sends only while its connection is held: a NULL conn would notify every connection
    blecon_assert(conn != NULL);

    if( !bt_gatt_is_subscribed(conn, zephyr_gatts_bearer->attr, BT_GATT_CCC_NOTIFY) ) {
        // A failure here could mean the client has disconnected, but the disconnection event hasn't propagated yet
        // (Zephyr reads no subscription once the link has left its connected state). So only return here
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

    int ret = bt_gatt_notify_cb(conn, &params);
    if( ret != 0 ) {
        // The link left its connected state since the check above: Zephyr's priority handler of the controller's
        // Disconnection Complete runs in the HCI driver's context, not under the event loop's lock. The notification
        // then fails with -ENOTCONN (bt_gatt_notify_cb's check, or bt_att_send's), -EINVAL (gatt_notify's own
        // subscription check, under Zephyr's default BT_GATT_ENFORCE_SUBSCRIPTION) or -ENOMEM (its PDU finding no ATT
        // channel). Dropped as an unsubscribed one is: the CCC's clearing closes the bearer. Any other failure, or any
        // of the three on a link Zephyr still reads connected (a PDU above the MTU, a peer's unsubscription between
        // the two checks), is fatal
        struct bt_conn_info conn_info = { 0 };
        int info_ret = bt_conn_get_info(conn, &conn_info);
        blecon_assert(info_ret == 0);
        blecon_assert((ret == -ENOTCONN) || (ret == -EINVAL) || (ret == -ENOMEM));
        blecon_assert(conn_info.state != BT_CONN_STATE_CONNECTED);
    }

    blecon_buffer_free(buf);
}

void blecon_zephyr_gatts_bearer_close(struct blecon_bearer_t* bearer, void* user_data) {
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = (struct blecon_zephyr_bluetooth_gatt_server_bearer_t*) user_data;
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = zephyr_gatts_bearer->bluetooth;

    if( !zephyr_gatts_bearer->connected ) {
        return;
    }

    // A second close before the task has run leaves one queued (the fragmentation layer closes its bearer on an
    // oversized record, and the layers above close it too)
    blecon_task_cancel(&zephyr_gatts_bearer->close_task);
    blecon_scheduler_queue_task(zephyr_bluetooth->bluetooth.scheduler, &zephyr_gatts_bearer->close_task);
}

ssize_t blecon_zephyr_gatts_bearer_gatt_write(struct bt_conn* conn, const struct bt_gatt_attr* attr, const void* buf, uint16_t len, uint16_t offset, uint8_t flags) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = _zephyr_bluetooth;
    size_t bearer_idx = (size_t) attr->user_data;
    struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer = &zephyr_bluetooth->gatts.bearers[bearer_idx];

    blecon_event_loop_lock(zephyr_bluetooth->event_loop);

    // Ignore if not a Blecon connection
    if( conn != zephyr_bluetooth->connection.conn ) {
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
    if( now_connected == zephyr_gatts_bearer->connected ) {
        blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
        return;
    }
    if( now_connected ) {
        zephyr_gatts_bearer->connected = true;
        blecon_bearer_on_open(&zephyr_gatts_bearer->bearer);
    } else {
        // The peer's write, or Zephyr clearing the CCC of a peer it holds no bond with at the disconnection
        blecon_zephyr_gatts_bearer_set_closed(zephyr_gatts_bearer);
    }
    blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
}

ssize_t blecon_zephyr_gatts_bearer_gatt_cccd_write(struct bt_conn* conn, const struct bt_gatt_attr* attr, uint16_t value) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = _zephyr_bluetooth;

    // The Zephyr host stack performs the following operations when handling a write to the CCCD (bt_gatt_attr_write_ccc):
    // * Claim a configuration slot for the peer, if it has none (a write of 0 from a peer without one ends here)
    // * Call this write callback with the new value
    // * Abort if the write is refused, the slot left claimed (released by blecon_zephyr_gatts_bearer_gatt_cccd_refuse)
    // * Store the new value
    // * Call the CCCD changed callback with the highest value of every connected peer's
    // Therefore, this callback only decides whether the write is accepted: the value is one the characteristic allows,
    // and the write is on the Blecon connection. The bearer opens or closes in the changed callback, as the new value
    // hasn't been stored yet here: bt_gatt_is_subscribed() would return false at this stage, and
    // blecon_zephyr_gatts_bearer_send() would drop what the bearer's user sent from its on_open

    // Notifications or nothing, as the characteristic declares notifications only. Zephyr checks no value against the
    // characteristic's properties, and would store indications too, which the changed callback reads as the bearer
    // closed: refused with NimBLE's answer to a bit its characteristic does not allow
    if( (value != 0) && (value != BT_GATT_CCC_NOTIFY) ) {
        return blecon_zephyr_gatts_bearer_gatt_cccd_refuse(conn, attr, BT_ATT_ERR_NOT_SUPPORTED);
    }

    blecon_event_loop_lock(zephyr_bluetooth->event_loop);
    bool blecon_connection = (conn == zephyr_bluetooth->connection.conn);
    blecon_event_loop_unlock(zephyr_bluetooth->event_loop);

    // Not a Blecon connection: Zephyr's changed callback reports the highest value of every connected peer's, so a
    // subscription on another connection would read as the Blecon one's
    if( !blecon_connection ) {
        return blecon_zephyr_gatts_bearer_gatt_cccd_refuse(conn, attr, BT_ATT_ERR_APPLICATION_0);
    }

    return sizeof(value);
}

// Refuses a CCCD write, and releases the configuration slot Zephyr claimed for it. bt_gatt_attr_write_ccc claims a slot
// for the peer (its address and identity) before it asks the write callback, and frees a slot only when its peer writes
// 0, or at its peer's disconnection if the slot's value is set: a refused write's slot, at 0, would stay claimed until
// that peer wrote again, and BT_GATT_CCC_MAX refusals from distinct peers would leave no slot for any subscription, the
// Blecon one's answered with Insufficient Resources until reboot. A peer's slot at 0 is only ever a refusal's (Zephyr
// clears one written to 0), so it is reset as Zephyr's own clear_ccc_cfg does; a slot holding a value (a refused
// rewrite of an existing subscription) is the peer's subscription, and is kept
ssize_t blecon_zephyr_gatts_bearer_gatt_cccd_refuse(struct bt_conn* conn, const struct bt_gatt_attr* attr, uint8_t att_err) {
    struct bt_gatt_ccc_managed_user_data* ccc = attr->user_data;

    struct bt_conn_info conn_info = { 0 };
    int ret = bt_conn_get_info(conn, &conn_info);
    blecon_assert(ret == 0);

    for( size_t idx = 0; idx < ARRAY_SIZE(ccc->cfg); idx++ ) {
        struct bt_gatt_ccc_cfg* cfg = &ccc->cfg[idx];
        if( (cfg->value == 0) && (cfg->id == conn_info.id) && bt_addr_le_eq(&cfg->peer, conn_info.le.dst) ) {
            bt_addr_le_copy(&cfg->peer, BT_ADDR_LE_ANY);
            cfg->id = 0;
            cfg->value = 0;
        }
    }

    return BT_GATT_ERR(att_err);
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

    // Queued only on an open bearer, and cancelled by its every close
    blecon_zephyr_gatts_bearer_set_closed(zephyr_gatts_bearer);
}

// Closes an open bearer: its user's close, if queued, is dropped (it would close the bearer's next opening), then
// on_closed is raised
void blecon_zephyr_gatts_bearer_set_closed(struct blecon_zephyr_bluetooth_gatt_server_bearer_t* zephyr_gatts_bearer) {
    blecon_assert(zephyr_gatts_bearer->connected);
    zephyr_gatts_bearer->connected = false;
    blecon_task_cancel(&zephyr_gatts_bearer->close_task);
    blecon_bearer_on_closed(&zephyr_gatts_bearer->bearer);
}
