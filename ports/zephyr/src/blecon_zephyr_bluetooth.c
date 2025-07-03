/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stdlib.h"
#include "string.h"

#include "blecon_zephyr_bluetooth.h"
#include "blecon_zephyr_bluetooth_common.h"
#include "blecon_zephyr_l2cap_bearer.h"
#include "blecon_zephyr_l2cap_server.h"
#include "blecon_zephyr_gatts_bearer.h"
#include "blecon_zephyr_event_loop.h"
#include "blecon/blecon_defs.h"
#include "blecon/blecon_memory.h"
#include "blecon/blecon_error.h"
#include "blecon/port/blecon_event_loop.h"

#include "zephyr/bluetooth/bluetooth.h"
#include "zephyr/bluetooth/conn.h"
#include "zephyr/bluetooth/gap.h"
#include "zephyr/bluetooth/hci.h"
#include "zephyr/random/random.h"
#include "zephyr/sys/byteorder.h"

#if defined(CONFIG_BT_CTLR_TX_PWR_PLUS_8)
#define BLUETOOTH_TX_POWER 8
#elif defined(CONFIG_BT_CTLR_TX_PWR_PLUS_7)
#define BLUETOOTH_TX_POWER 7
#elif defined(CONFIG_BT_CTLR_TX_PWR_PLUS_6)
#define BLUETOOTH_TX_POWER 7
#elif defined(CONFIG_BT_CTLR_TX_PWR_PLUS_5)
#define BLUETOOTH_TX_POWER 5
#elif defined(CONFIG_BT_CTLR_TX_PWR_PLUS_4)
#define BLUETOOTH_TX_POWER 4
#elif defined(CONFIG_BT_CTLR_TX_PWR_PLUS_3)
#define BLUETOOTH_TX_POWER 3
#elif defined(CONFIG_BT_CTLR_TX_PWR_PLUS_2)
#define BLUETOOTH_TX_POWER 2
#elif defined(CONFIG_BT_CTLR_TX_PWR_0)
#define BLUETOOTH_TX_POWER 0
#elif defined(CONFIG_BT_CTLR_TX_PWR_MINUS_4)
#define BLUETOOTH_TX_POWER -4
#elif defined(CONFIG_BT_CTLR_TX_PWR_MINUS_8)
#define BLUETOOTH_TX_POWER -8
#elif defined(CONFIG_BT_CTLR_TX_PWR_MINUS_12)
#define BLUETOOTH_TX_POWER -12
#elif defined(CONFIG_BT_CTLR_TX_PWR_MINUS_16)
#define BLUETOOTH_TX_POWER -16
#elif defined(CONFIG_BT_CTLR_TX_PWR_MINUS_20)
#define BLUETOOTH_TX_POWER -20
#elif defined(CONFIG_BT_CTLR_TX_PWR_MINUS_30)
#define BLUETOOTH_TX_POWER -30
#elif defined(CONFIG_BT_CTLR_TX_PWR_MINUS_40)
#define BLUETOOTH_TX_POWER -40
#else
#warning "No TX power defined"
#define BLUETOOTH_TX_POWER 0
#endif


struct blecon_zephyr_bluetooth_t;

static void blecon_zephyr_bluetooth_setup(struct blecon_bluetooth_t* bluetooth);
static void blecon_zephyr_bluetooth_shutdown(struct blecon_bluetooth_t* bluetooth);
static struct blecon_bluetooth_advertising_set_t* blecon_zephyr_bluetooth_advertising_set_new(struct blecon_bluetooth_t* bluetooth);
static void blecon_zephyr_bluetooth_advertising_set_update_params(struct blecon_bluetooth_advertising_set_t* adv_set, struct blecon_bluetooth_advertising_params_t* params);
static void blecon_zephyr_bluetooth_advertising_set_update_data(struct blecon_bluetooth_advertising_set_t* adv_set, struct blecon_bluetooth_advertising_data_t* data);
static void blecon_zephyr_bluetooth_advertising_set_start(struct blecon_bluetooth_advertising_set_t* adv_set);
static void blecon_zephyr_bluetooth_advertising_set_stop(struct blecon_bluetooth_advertising_set_t* adv_set);
static void blecon_zephyr_bluetooth_advertising_set_free(struct blecon_bluetooth_advertising_set_t* adv_set);
static void blecon_zephyr_bluetooth_get_address(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_addr_t* bt_addr);
static void blecon_zephyr_bluetooth_connection_get_info(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_connection_info_t* info);
static void blecon_zephyr_bluetooth_connection_get_power_info(struct blecon_bluetooth_connection_t* connection, int8_t* tx_power, int8_t* rssi);
static void blecon_zephyr_bluetooth_connection_disconnect(struct blecon_bluetooth_connection_t* connection);
static void blecon_zephyr_bluetooth_connection_free(struct blecon_bluetooth_connection_t* connection);
static void blecon_zephyr_bluetooth_scan_start(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_phy_mask_t phy_mask, bool active_scan);
static void blecon_zephyr_bluetooth_scan_stop(struct blecon_bluetooth_t* bluetooth);

static void blecon_zephyr_bluetooth_on_connected(struct bt_conn* conn, uint8_t conn_err);
static void blecon_zephyr_bluetooth_on_disconnected(struct bt_conn* conn, uint8_t reason);
static void blecon_zephyr_bluetooth_on_scan_report_received(const struct bt_le_scan_recv_info* info, struct net_buf_simple* buf);
static void ble_read_conn_rssi(uint16_t handle, int8_t* rssi);

static struct bt_conn_cb zephyr_bluetooth_callbacks = {
    .connected = blecon_zephyr_bluetooth_on_connected,
    .disconnected = blecon_zephyr_bluetooth_on_disconnected
};

static struct bt_le_scan_cb zephyr_bluetooth_scan_callbacks = {
	.recv = blecon_zephyr_bluetooth_on_scan_report_received,
};

// Singleton
static struct blecon_zephyr_bluetooth_t* _zephyr_bluetooth = NULL;

struct blecon_bluetooth_t* blecon_zephyr_bluetooth_init(struct blecon_event_loop_t* event_loop) {
    static const struct blecon_bluetooth_fn_t bluetooth_fn = {
        .setup = blecon_zephyr_bluetooth_setup,
        .shutdown = blecon_zephyr_bluetooth_shutdown,
        .advertising_set_new = blecon_zephyr_bluetooth_advertising_set_new,
        .advertising_set_update_params = blecon_zephyr_bluetooth_advertising_set_update_params,
        .advertising_set_update_data = blecon_zephyr_bluetooth_advertising_set_update_data,
        .advertising_set_start = blecon_zephyr_bluetooth_advertising_set_start,
        .advertising_set_stop = blecon_zephyr_bluetooth_advertising_set_stop,
        .advertising_set_free = blecon_zephyr_bluetooth_advertising_set_free,
        .get_address = blecon_zephyr_bluetooth_get_address,
        .l2cap_server_new = blecon_zephyr_bluetooth_l2cap_server_new,
        .gatt_server_new = blecon_zephyr_bluetooth_gatt_server_new,
        .connection_get_info = blecon_zephyr_bluetooth_connection_get_info,
        .connection_get_power_info = blecon_zephyr_bluetooth_connection_get_power_info,
        .connection_disconnect = blecon_zephyr_bluetooth_connection_disconnect,
        .connection_get_l2cap_server_bearer = blecon_zephyr_bluetooth_connection_get_l2cap_server_bearer,
        .connection_get_gatt_server_bearer = blecon_zephyr_bluetooth_connection_get_gatt_server_bearer,
        .connection_free = blecon_zephyr_bluetooth_connection_free,
        .scan_start = blecon_zephyr_bluetooth_scan_start,
        .scan_stop = blecon_zephyr_bluetooth_scan_stop
    };

    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = BLECON_ALLOC(sizeof(struct blecon_zephyr_bluetooth_t));
    if(zephyr_bluetooth == NULL) {
        blecon_fatal_error();
    }

    blecon_assert(_zephyr_bluetooth == NULL);
    _zephyr_bluetooth = zephyr_bluetooth;

    blecon_bluetooth_init(&zephyr_bluetooth->bluetooth, &bluetooth_fn);

    zephyr_bluetooth->event_loop = event_loop;
    for(size_t idx = 0; idx < BLECON_MAX_ADVERTISING_SETS; idx++) {
        zephyr_bluetooth->adv_sets[idx].identity = -1;
        zephyr_bluetooth->adv_sets[idx].handle = NULL;
    }
    zephyr_bluetooth->adv_sets_count = 0;

    zephyr_bluetooth->connection.conn = NULL;

    blecon_zephyr_bluetooth_gatt_server_init(zephyr_bluetooth);

    return &zephyr_bluetooth->bluetooth;
}

void blecon_zephyr_bluetooth_setup(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = (struct blecon_zephyr_bluetooth_t*) bluetooth;
    if(!bt_is_ready()) {
        int ret = bt_enable(NULL);
        blecon_assert(ret == 0);
    }

    // Set initial address (random)
    // Get random bytes
    if( sys_csrand_get(zephyr_bluetooth->bt_addr, BLECON_BLUETOOTH_ADDR_SZ) != 0 ) {
        blecon_fatal_error();
    }

    zephyr_bluetooth->bt_addr[5] |= 0xc0; // Random static address

    bt_conn_cb_register(&zephyr_bluetooth_callbacks);
    bt_le_scan_cb_register(&zephyr_bluetooth_scan_callbacks);
}

void blecon_zephyr_bluetooth_shutdown(struct blecon_bluetooth_t* bluetooth) {

}

struct blecon_bluetooth_advertising_set_t* blecon_zephyr_bluetooth_advertising_set_new(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = (struct blecon_zephyr_bluetooth_t*) bluetooth;
    blecon_assert(zephyr_bluetooth->adv_sets_count < BLECON_MAX_ADVERTISING_SETS);

    struct blecon_zephyr_bluetooth_advertising_set_t* adv_set = &zephyr_bluetooth->adv_sets[zephyr_bluetooth->adv_sets_count];
    zephyr_bluetooth->adv_sets_count++;

    return &adv_set->set;
}

void blecon_zephyr_bluetooth_advertising_set_update_params(struct blecon_bluetooth_advertising_set_t* adv_set, struct blecon_bluetooth_advertising_params_t* params) {
    struct blecon_zephyr_bluetooth_advertising_set_t* zephyr_adv_set = (struct blecon_zephyr_bluetooth_advertising_set_t*) adv_set;
    
    // Set advertising address
    bt_addr_le_t bt_addr = {0};
    bt_addr.type = params->bt_addr.addr_type;
    memcpy(bt_addr.a.val, params->bt_addr.bytes, BLECON_BLUETOOTH_ADDR_SZ);

    // Create identity
    int ret = 0;
    if(zephyr_adv_set->identity != -1) {
        // Re-use existing identity
        ret = bt_id_reset(zephyr_adv_set->identity, &bt_addr, NULL);
        blecon_assert( (ret >= 0) || (ret == -EALREADY) );
    } else {
        // Create new identity
        ret = bt_id_create(&bt_addr, NULL);
        blecon_assert(ret >= 0);
    }
    if( ret >= 0 ) {
        zephyr_adv_set->identity = ret;
    }

    struct bt_le_adv_param adv_param = {
        .id = zephyr_adv_set->identity,
        .sid = params->sid,
        .secondary_max_skip = 0U,
        .options = 0, // Set below
        .interval_min = params->interval_0_625ms,
        .interval_max = params->interval_0_625ms,
        .peer = NULL,
    };

    // Set options
    // Use identity address
    adv_param.options |= BT_LE_ADV_OPT_USE_IDENTITY;
    
    if(!params->legacy_pdu) {
        adv_param.options |= BT_LE_ADV_OPT_EXT_ADV;
    }

    // Use legacy PDU for 1M PHY, extended PDU otherwise
    if(params->phy == blecon_bluetooth_phy_coded) {
        adv_param.options |= BT_LE_ADV_OPT_CODED;
    }

    if(params->is_connectable) {
        adv_param.options |= BT_LE_ADV_OPT_CONNECTABLE;
    }

    // Mandatory to be scannable when connectable and using legacy PDUs
    if(params->is_connectable && params->legacy_pdu) {
        adv_param.options |= BT_LE_ADV_OPT_SCANNABLE;
    }

    // Set advertising parameters
    if(zephyr_adv_set->handle != NULL) {
        // Update advertising set
        ret = bt_le_ext_adv_update_param(zephyr_adv_set->handle, &adv_param);
    } else {
        // Create advertising set
        ret = bt_le_ext_adv_create(&adv_param, NULL, &zephyr_adv_set->handle);
    }
    blecon_assert(ret == 0);

    // Update TX power
    params->tx_power = BLUETOOTH_TX_POWER;
}

void blecon_zephyr_bluetooth_advertising_set_update_data(struct blecon_bluetooth_advertising_set_t* adv_set, struct blecon_bluetooth_advertising_data_t* data) {
    struct blecon_zephyr_bluetooth_advertising_set_t* zephyr_adv_set = (struct blecon_zephyr_bluetooth_advertising_set_t*) adv_set;

    blecon_assert(zephyr_adv_set->handle != NULL);

    // Set data - bit annoying, we have to re-parse our encoded advertising packet
    struct bt_data z_adv_data[16] = {0};
    size_t z_adv_data_sz = 0;
    size_t adv_data_pos = 0;
    while(adv_data_pos < data->data_sz) {
        z_adv_data[z_adv_data_sz].data_len = data->data[adv_data_pos++] - 1;
        z_adv_data[z_adv_data_sz].type = data->data[adv_data_pos++];
        z_adv_data[z_adv_data_sz].data = &data->data[adv_data_pos];
        blecon_assert(z_adv_data[z_adv_data_sz].data_len <= data->data_sz - adv_data_pos);
        adv_data_pos += z_adv_data[z_adv_data_sz].data_len;
        z_adv_data_sz++;
    }

    int ret = bt_le_ext_adv_set_data(zephyr_adv_set->handle, z_adv_data, z_adv_data_sz, NULL, 0);
    blecon_assert(ret == 0);
}

void blecon_zephyr_bluetooth_advertising_set_start(struct blecon_bluetooth_advertising_set_t* adv_set) {
    struct blecon_zephyr_bluetooth_advertising_set_t* zephyr_adv_set = (struct blecon_zephyr_bluetooth_advertising_set_t*) adv_set;

    int ret = bt_le_ext_adv_start(zephyr_adv_set->handle, BT_LE_EXT_ADV_START_DEFAULT);
    blecon_assert(ret == 0);
}

void blecon_zephyr_bluetooth_advertising_set_stop(struct blecon_bluetooth_advertising_set_t* adv_set) {
    struct blecon_zephyr_bluetooth_advertising_set_t* zephyr_adv_set = (struct blecon_zephyr_bluetooth_advertising_set_t*) adv_set;
    
    // BLE spec says that disabling an already disabled adv set has no effect
    // So should be fine to call that even if it's already disabled
    // (because of a connection for instance)
    int ret = bt_le_ext_adv_stop(zephyr_adv_set->handle);
    blecon_assert(ret == 0);
}

void blecon_zephyr_bluetooth_advertising_set_free(struct blecon_bluetooth_advertising_set_t* adv_set) {
    blecon_fatal_error(); // Not allowed
}

void blecon_zephyr_bluetooth_get_address(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_addr_t* bt_addr) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = (struct blecon_zephyr_bluetooth_t*) bluetooth;
    bt_addr->addr_type = BT_ADDR_LE_RANDOM;
    memcpy(bt_addr->bytes, zephyr_bluetooth->bt_addr, BLECON_BLUETOOTH_ADDR_SZ);
}

void blecon_zephyr_bluetooth_connection_get_info(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_connection_info_t* info) {
    struct blecon_zephyr_bluetooth_connection_t* zephyr_connection = (struct blecon_zephyr_bluetooth_connection_t*) connection;

    struct bt_conn_info conn_info = {0};
    int ret = bt_conn_get_info(zephyr_connection->conn, &conn_info);
	blecon_assert(ret == 0);

    // Addresses
    info->our_bt_addr.addr_type = conn_info.le.local->type;
    memcpy(info->our_bt_addr.bytes, conn_info.le.local->a.val, BLECON_BLUETOOTH_ADDR_SZ);
    info->peer_bt_addr.addr_type = conn_info.le.remote->type;
    memcpy(info->peer_bt_addr.bytes, conn_info.le.remote->a.val, BLECON_BLUETOOTH_ADDR_SZ);

    // Role (central/peripheral)
    info->is_central = conn_info.role == BT_CONN_ROLE_CENTRAL;

    // PHY
    switch(conn_info.le.phy->rx_phy) {
        case BT_GAP_LE_PHY_CODED:
            info->phy = blecon_bluetooth_phy_coded;
            break;
        case BT_GAP_LE_PHY_2M:
            info->phy = blecon_bluetooth_phy_2m;
            break;
        case BT_GAP_LE_PHY_1M:
        default:
            info->phy = blecon_bluetooth_phy_1m;
            break;
    }
}

void blecon_zephyr_bluetooth_connection_get_power_info(struct blecon_bluetooth_connection_t* connection, int8_t* tx_power, int8_t* rssi) {
    struct blecon_zephyr_bluetooth_connection_t* zephyr_connection = (struct blecon_zephyr_bluetooth_connection_t*) connection;

    uint16_t conn_handle = 0;
    int ret = bt_hci_get_conn_handle(zephyr_connection->conn, &conn_handle);
    blecon_assert(ret == 0);

    *tx_power = BLUETOOTH_TX_POWER;

    ble_read_conn_rssi(conn_handle, rssi);
}

void blecon_zephyr_bluetooth_connection_disconnect(struct blecon_bluetooth_connection_t* connection) {
    struct blecon_zephyr_bluetooth_connection_t* zephyr_connection = (struct blecon_zephyr_bluetooth_connection_t*) connection;

    int ret = bt_conn_disconnect(zephyr_connection->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    blecon_assert( (ret == 0) || (ret == -ENOTCONN) || (ret == -EIO) ); // EIO can be returned if connection was closed already
}

void blecon_zephyr_bluetooth_connection_free(struct blecon_bluetooth_connection_t* connection) {

}

void blecon_zephyr_bluetooth_scan_start(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_phy_mask_t phy_mask, bool active_scan) {
    // Set scan parameters
    struct bt_le_scan_param scan_param = {
        .type = active_scan ? BT_HCI_LE_SCAN_ACTIVE : BT_HCI_LE_SCAN_PASSIVE,
        .options = BT_LE_SCAN_OPT_NONE,
        .interval = 128,
        .window = 128,
        .timeout = 0, // No timeout
        .interval_coded = 0,
        .window_coded = 0,
    };

    if(phy_mask.phy_coded) {
        scan_param.options |= BT_LE_SCAN_OPT_CODED;
    }
    if(!phy_mask.phy_1m) {
        scan_param.options |= BT_LE_SCAN_OPT_NO_1M;
    }

    int ret = bt_le_scan_start(&scan_param, NULL);
    blecon_assert(ret == 0);
}

void blecon_zephyr_bluetooth_scan_stop(struct blecon_bluetooth_t* bluetooth) {
    int ret = bt_le_scan_stop();
    blecon_assert(ret == 0);
}

void blecon_zephyr_bluetooth_on_connected(struct bt_conn* conn, uint8_t conn_err) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = _zephyr_bluetooth;

    if( conn_err != 0 ) {
        return;
    }

    // Ignore central connections
    struct bt_conn_info conn_info = {0};
    int ret = bt_conn_get_info(conn, &conn_info);
	blecon_assert(ret == 0);

	if(conn_info.role != BT_CONN_ROLE_PERIPHERAL) {
        return;
    }

    // Lock event loop
    blecon_event_loop_lock(zephyr_bluetooth->event_loop);

    // Find advertising set based on connection identity
    struct blecon_zephyr_bluetooth_advertising_set_t* zephyr_adv_set = NULL;
    for(size_t idx = 0; idx < zephyr_bluetooth->adv_sets_count; idx++) {
        if(zephyr_bluetooth->adv_sets[idx].identity == conn_info.id) {
            zephyr_adv_set = &zephyr_bluetooth->adv_sets[idx];
            break;
        }
    }

    if(zephyr_adv_set == NULL) {
        blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
        return;
    }

    // Update connection params
    const static struct bt_le_conn_param conn_params = {
        .interval_min = 12, // 1.25 ms units = 15 ms
        .interval_max = 12,
        .latency = 0,
        .timeout = 400, // 10 ms units
    };
    ret = bt_conn_le_param_update(conn, &conn_params);
    blecon_assert(ret == 0);

    // Update length
    const static struct bt_conn_le_data_len_param len_params = {
        .tx_max_len = 251,
        .tx_max_time = 2120
    };
    ret = bt_conn_le_data_len_update(conn, &len_params);
    blecon_assert(ret == 0);

    // Init
    blecon_bluetooth_connection_init(&zephyr_bluetooth->connection.connection, &zephyr_bluetooth->bluetooth);
    zephyr_bluetooth->connection.conn = bt_conn_ref(conn);
    blecon_bluetooth_on_new_connection(&zephyr_bluetooth->bluetooth, 
        &zephyr_bluetooth->connection.connection, &zephyr_adv_set->set);
    blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
}

void blecon_zephyr_bluetooth_on_disconnected(struct bt_conn* conn, uint8_t reason) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = _zephyr_bluetooth;
    
    blecon_event_loop_lock(zephyr_bluetooth->event_loop);

    // Ignore central connections
    if( conn != zephyr_bluetooth->connection.conn ) {
        blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
        return;
    }

    bt_conn_unref(zephyr_bluetooth->connection.conn);
    zephyr_bluetooth->connection.conn = NULL;
    blecon_bluetooth_connection_on_disconnected(&zephyr_bluetooth->connection.connection);
    blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
}

void blecon_zephyr_bluetooth_on_scan_report_received(const struct bt_le_scan_recv_info* info, struct net_buf_simple* buf) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = _zephyr_bluetooth;

    blecon_event_loop_lock(zephyr_bluetooth->event_loop);

    struct blecon_bluetooth_advertising_info_t adv_info = {
        .bt_addr.addr_type = info->addr->type,
        .legacy_pdu = (info->adv_props & BT_GAP_ADV_PROP_EXT_ADV) == 0,
        .is_connectable = (info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) != 0,
        .is_scan_response = (info->adv_props & BT_GAP_ADV_PROP_SCAN_RESPONSE) != 0,
        .sid = info->sid,
        .tx_power = info->tx_power,
        .rssi = info->rssi,
        .phy = (info->primary_phy == BT_GAP_LE_PHY_CODED) ? blecon_bluetooth_phy_coded : 
            (info->primary_phy == BT_GAP_LE_PHY_2M) ? blecon_bluetooth_phy_2m : 
            blecon_bluetooth_phy_1m
    };
    memcpy(adv_info.bt_addr.bytes, info->addr->a.val, BLECON_BLUETOOTH_ADDR_SZ);

    struct blecon_bluetooth_advertising_data_t adv_data = {
        .data = buf->data,
        .data_sz = buf->len,
    };

    blecon_bluetooth_on_advertising_report(&zephyr_bluetooth->bluetooth, &adv_info, &adv_data);
    blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
}

// Polyfill from https://github.com/nrfconnect/sdk-zephyr/blob/main/samples/bluetooth/hci_pwr_ctrl/src/main.c
static void ble_read_conn_rssi(uint16_t handle, int8_t* rssi)
{
	struct net_buf *buf, *rsp = NULL;
	struct bt_hci_cp_read_rssi *cp;
	struct bt_hci_rp_read_rssi *rp;

	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_READ_RSSI, sizeof(*cp));
	if (!buf) {
		return;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_RSSI, buf, &rsp);
	if (err) {
		return;
	}

	rp = (void *)rsp->data;
	*rssi = rp->rssi;

	net_buf_unref(rsp);
}