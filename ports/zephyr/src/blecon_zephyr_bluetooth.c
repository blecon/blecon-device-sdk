/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stdlib.h"
#include "string.h"
#include "assert.h"

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
#include "zephyr/random/rand32.h"
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
static int32_t blecon_zephyr_bluetooth_advertising_tx_power(struct blecon_bluetooth_t* bluetooth);
static int32_t blecon_zephyr_bluetooth_connection_tx_power(struct blecon_bluetooth_t* bluetooth);
static int32_t blecon_zephyr_bluetooth_connection_rssi(struct blecon_bluetooth_t* bluetooth);
static enum blecon_bluetooth_phy_t blecon_zephyr_bluetooth_connection_phy(struct blecon_bluetooth_t* bluetooth);
static void blecon_zephyr_bluetooth_advertising_set_data(
        struct blecon_bluetooth_t* bluetooth,
        const struct blecon_bluetooth_advertising_set_t* adv_sets,
        size_t adv_sets_count
    );
static bool blecon_zephyr_bluetooth_advertising_start(struct blecon_bluetooth_t* bluetooth, uint32_t adv_interval_ms);
static void blecon_zephyr_bluetooth_advertising_stop(struct blecon_bluetooth_t* bluetooth);
static void blecon_zephyr_bluetooth_rotate_mac_address(struct blecon_bluetooth_t* bluetooth);
static void blecon_zephyr_bluetooth_get_mac_address(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_addr_t* bt_mac_addr);
static void blecon_zephyr_bluetooth_disconnect(struct blecon_bluetooth_t* bluetooth);
static void blecon_zephyr_bluetooth_advertising_start_internal(struct blecon_zephyr_bluetooth_t* zephyr_bluetooth);
static void blecon_zephyr_bluetooth_advertising_stop_internal(struct blecon_zephyr_bluetooth_t* zephyr_bluetooth);
static void blecon_zephyr_bluetooth_on_connected(struct bt_conn* conn, uint8_t conn_err);
static void blecon_zephyr_bluetooth_on_disconnected(struct bt_conn* conn, uint8_t reason);
static void ble_read_conn_rssi(uint16_t handle, int8_t* rssi);

static struct bt_conn_cb zephyr_bluetooth_callbacks = {
    .connected = blecon_zephyr_bluetooth_on_connected,
    .disconnected = blecon_zephyr_bluetooth_on_disconnected
};

// Singleton
static struct blecon_zephyr_bluetooth_t* _zephyr_bluetooth = NULL;

struct blecon_bluetooth_t* blecon_zephyr_bluetooth_init(struct blecon_event_loop_t* event_loop) {
    static const struct blecon_bluetooth_fn_t bluetooth_fn = {
        .setup = blecon_zephyr_bluetooth_setup,
        .advertising_tx_power = blecon_zephyr_bluetooth_advertising_tx_power,
        .connection_tx_power = blecon_zephyr_bluetooth_connection_tx_power,
        .connection_rssi = blecon_zephyr_bluetooth_connection_rssi,
        .connection_phy = blecon_zephyr_bluetooth_connection_phy,
        .advertising_set_data = blecon_zephyr_bluetooth_advertising_set_data,
        .advertising_start = blecon_zephyr_bluetooth_advertising_start,
        .advertising_stop = blecon_zephyr_bluetooth_advertising_stop,
        .rotate_mac_address = blecon_zephyr_bluetooth_rotate_mac_address,
        .get_mac_address = blecon_zephyr_bluetooth_get_mac_address,
        .disconnect = blecon_zephyr_bluetooth_disconnect,
        .l2cap_server_new = blecon_zephyr_bluetooth_l2cap_server_new,
        .l2cap_server_as_bearer = blecon_zephyr_bluetooth_l2cap_server_as_bearer,
        .l2cap_server_free = blecon_zephyr_bluetooth_l2cap_server_free,
        .gatts_bearer_new = blecon_zephyr_bluetooth_gatts_bearer_new,
        .gatts_bearer_as_bearer = blecon_zephyr_bluetooth_gatts_bearer_as_bearer,
        .gatts_bearer_free = blecon_zephyr_bluetooth_gatts_bearer_free
    };

    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = BLECON_ALLOC(sizeof(struct blecon_zephyr_bluetooth_t));
    if(zephyr_bluetooth == NULL) {
        blecon_fatal_error();
    }

    blecon_assert(_zephyr_bluetooth == NULL);
    _zephyr_bluetooth = zephyr_bluetooth;

    blecon_bluetooth_init(&zephyr_bluetooth->bluetooth, &bluetooth_fn);

    zephyr_bluetooth->event_loop = event_loop;
    zephyr_bluetooth->advertising = false;
    for(size_t idx = 0; idx < BLECON_MAX_ADVERTISING_SETS; idx++) {
        zephyr_bluetooth->adv_sets[idx].identity = -1;
        zephyr_bluetooth->adv_sets[idx].handle = NULL;
    }
    zephyr_bluetooth->adv_sets_count = 0;
    zephyr_bluetooth->adv_interval_ms = 0;
    zephyr_bluetooth->conn = NULL;

    blecon_zephyr_bluetooth_gatts_init(zephyr_bluetooth);

    return &zephyr_bluetooth->bluetooth;
}

void blecon_zephyr_bluetooth_setup(struct blecon_bluetooth_t* bluetooth) {
    if(!bt_is_ready()) {
        int ret = bt_enable(NULL);
        blecon_assert(ret == 0);
    }

    // Set initial address (random)
    blecon_zephyr_bluetooth_rotate_mac_address(bluetooth);

    bt_conn_cb_register(&zephyr_bluetooth_callbacks);
}

int32_t blecon_zephyr_bluetooth_advertising_tx_power(struct blecon_bluetooth_t* bluetooth) {
    return BLUETOOTH_TX_POWER;
}

int32_t blecon_zephyr_bluetooth_connection_tx_power(struct blecon_bluetooth_t* bluetooth) {
    return BLUETOOTH_TX_POWER;
}

int32_t blecon_zephyr_bluetooth_connection_rssi(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = (struct blecon_zephyr_bluetooth_t*) bluetooth;
    blecon_assert(zephyr_bluetooth->conn != NULL);

    uint16_t conn_handle = 0;
    int ret = bt_hci_get_conn_handle(zephyr_bluetooth->conn, &conn_handle);
    blecon_assert(ret == 0);

    int8_t rssi = 0;
    ble_read_conn_rssi(conn_handle, &rssi);
    
    return rssi;
}

static enum blecon_bluetooth_phy_t blecon_zephyr_bluetooth_connection_phy(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = (struct blecon_zephyr_bluetooth_t*) bluetooth;
    blecon_assert(zephyr_bluetooth->conn != NULL);

    struct bt_conn_info conn_info = {0};
    int ret = bt_conn_get_info(zephyr_bluetooth->conn, &conn_info);
	blecon_assert(ret == 0);

    switch(conn_info.le.phy->rx_phy) {
        case BT_GAP_LE_PHY_CODED:
            return blecon_bluetooth_phy_coded;
        case BT_GAP_LE_PHY_2M:
            return blecon_bluetooth_phy_2m;
        case BT_GAP_LE_PHY_1M:
        default:
            return blecon_bluetooth_phy_1m;
    }
}

void blecon_zephyr_bluetooth_advertising_set_data(
        struct blecon_bluetooth_t* bluetooth,
        const struct blecon_bluetooth_advertising_set_t* adv_sets,
        size_t adv_sets_count
    ) 
{
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = (struct blecon_zephyr_bluetooth_t*) bluetooth;
    blecon_assert(adv_sets_count <= BLECON_MAX_ADVERTISING_SETS);

    if(zephyr_bluetooth->advertising) {
        blecon_zephyr_bluetooth_advertising_stop_internal(zephyr_bluetooth);
    }

    // Copy new sets
    zephyr_bluetooth->adv_sets_count = adv_sets_count;
    for(size_t idx = 0; idx < zephyr_bluetooth->adv_sets_count; idx++) {
        memcpy(&zephyr_bluetooth->adv_sets[idx].set, &adv_sets[idx], sizeof(struct blecon_bluetooth_advertising_set_t));
        zephyr_bluetooth->adv_sets[idx].handle = NULL;
    }

    // Restart advertising
    if( zephyr_bluetooth->advertising ) {
        blecon_zephyr_bluetooth_advertising_start_internal(zephyr_bluetooth);
    }
}

bool blecon_zephyr_bluetooth_advertising_start(struct blecon_bluetooth_t* bluetooth, uint32_t adv_interval_ms) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = (struct blecon_zephyr_bluetooth_t*) bluetooth;
    blecon_assert(!zephyr_bluetooth->advertising);
    zephyr_bluetooth->advertising = true;
    zephyr_bluetooth->adv_interval_ms = adv_interval_ms;

    blecon_zephyr_bluetooth_advertising_start_internal(zephyr_bluetooth);

    return true;
}

void blecon_zephyr_bluetooth_advertising_stop(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = (struct blecon_zephyr_bluetooth_t*) bluetooth;

    blecon_zephyr_bluetooth_advertising_stop_internal(zephyr_bluetooth);

    zephyr_bluetooth->advertising = false;
}

void blecon_zephyr_bluetooth_rotate_mac_address(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = (struct blecon_zephyr_bluetooth_t*) bluetooth;

    // Get random bytes
    if( sys_csrand_get(zephyr_bluetooth->adv_base_addr, BLECON_BLUETOOTH_ADDR_SZ) != 0 ) {
        blecon_fatal_error();
    }

    zephyr_bluetooth->adv_base_addr[5] |= 0xc0; // Random static address
}

void blecon_zephyr_bluetooth_get_mac_address(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_addr_t* bt_mac_addr) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = (struct blecon_zephyr_bluetooth_t*) bluetooth;

    bt_mac_addr->addr_type = BT_ADDR_LE_RANDOM;
    memcpy(bt_mac_addr->bytes, zephyr_bluetooth->adv_base_addr, BLECON_BLUETOOTH_ADDR_SZ);
}

void blecon_zephyr_bluetooth_disconnect(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = (struct blecon_zephyr_bluetooth_t*) bluetooth;
    
    blecon_assert(zephyr_bluetooth->conn != NULL);
    
    int ret = bt_conn_disconnect(zephyr_bluetooth->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    blecon_assert( (ret == 0) || (ret == -ENOTCONN) || (ret == -EIO) ); // EIO can be returned if connection was closed already
}

void blecon_zephyr_bluetooth_advertising_start_internal(struct blecon_zephyr_bluetooth_t* zephyr_bluetooth) {
    // Create advertising sets
    for(size_t idx = 0; idx < zephyr_bluetooth->adv_sets_count; idx++) {
        const struct blecon_bluetooth_advertising_set_t* adv_set = &zephyr_bluetooth->adv_sets[idx].set;

        // Set advertising address
        bt_addr_le_t bt_addr = {0};
        bt_addr.type = BT_ADDR_LE_RANDOM;
        memcpy(bt_addr.a.val, zephyr_bluetooth->adv_base_addr, BLECON_BLUETOOTH_ADDR_SZ);
        bt_addr.a.val[0] += idx & 0xffu; // Rotate Address LSB

        // Create identity
        int ret = bt_id_create(&bt_addr, NULL);
        blecon_assert(ret >= 0);
        zephyr_bluetooth->adv_sets[idx].identity = ret;

        struct bt_le_adv_param adv_param = {
            .id = zephyr_bluetooth->adv_sets[idx].identity,
            .sid = idx, // Use index as SID
            .secondary_max_skip = 0U,
            .options = 0, // Set below
            .interval_min = (zephyr_bluetooth->adv_interval_ms * 1000u) / 625u,
            .interval_max = (zephyr_bluetooth->adv_interval_ms * 1000u) / 625u + 1,
            .peer = NULL,
        };

        // Set options
        // Use identity address
        adv_param.options |= BT_LE_ADV_OPT_USE_IDENTITY;

        // Use legacy PDU for 1M PHY, extended PDU otherwise
        if(adv_set->phy == blecon_bluetooth_phy_coded) {
            adv_param.options |= BT_LE_ADV_OPT_EXT_ADV;
            adv_param.options |= BT_LE_ADV_OPT_CODED;
        }

        if(adv_set->is_connectable) {
            adv_param.options |= BT_LE_ADV_OPT_CONNECTABLE;
        }

        // Mandatory to be scannable when connectable and using legacy PDUs
        if(adv_set->is_connectable && (adv_set->phy == blecon_bluetooth_phy_1m)) {
            adv_param.options |= BT_LE_ADV_OPT_SCANNABLE;
        }

        // Create advertising set
        ret = bt_le_ext_adv_create(&adv_param, NULL, &zephyr_bluetooth->adv_sets[idx].handle);
        blecon_assert(ret == 0);
        
        // Set data - bit annoying, we have to re-parse our encoded advertising packet
        struct bt_data z_adv_data[16] = {0};
        size_t z_adv_data_sz = 0;
        size_t adv_data_pos = 0;
        while(adv_data_pos < adv_set->adv_data_sz) {
            z_adv_data[z_adv_data_sz].data_len = adv_set->adv_data[adv_data_pos++] - 1;
            z_adv_data[z_adv_data_sz].type = adv_set->adv_data[adv_data_pos++];
            z_adv_data[z_adv_data_sz].data = &adv_set->adv_data[adv_data_pos];
            blecon_assert(z_adv_data[z_adv_data_sz].data_len <= adv_set->adv_data_sz - adv_data_pos);
            adv_data_pos += z_adv_data[z_adv_data_sz].data_len;
            z_adv_data_sz++;
        }

        ret = bt_le_ext_adv_set_data(zephyr_bluetooth->adv_sets[idx].handle, z_adv_data, z_adv_data_sz, NULL, 0);
        blecon_assert(ret == 0);
    }

    // Start advertising
    for(size_t idx = 0; idx < zephyr_bluetooth->adv_sets_count; idx++) {
        int ret = bt_le_ext_adv_start(zephyr_bluetooth->adv_sets[idx].handle, BT_LE_EXT_ADV_START_DEFAULT);
        blecon_assert(ret == 0);
    }
}

void blecon_zephyr_bluetooth_advertising_stop_internal(struct blecon_zephyr_bluetooth_t* zephyr_bluetooth) {
    // Clear existing sets (in reverse order so that identities are actually deleted)
    for(int32_t idx = zephyr_bluetooth->adv_sets_count - 1; idx >= 0; idx--) {
        // BLE spec says that disabling an already disabled adv set has no effect
        // So should be fine to call that even if it's already disabled
        // (because of a connection for instance)
        int ret = bt_le_ext_adv_stop(zephyr_bluetooth->adv_sets[idx].handle);
        blecon_assert(ret == 0);

        // Delete adv set
        ret = bt_le_ext_adv_delete(zephyr_bluetooth->adv_sets[idx].handle);
        blecon_assert(ret == 0);
        zephyr_bluetooth->adv_sets[idx].handle = NULL;

        // Delete identity
        ret = bt_id_delete(zephyr_bluetooth->adv_sets[idx].identity);
        blecon_assert(ret == 0);
        zephyr_bluetooth->adv_sets[idx].identity = -1;
    }
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

    blecon_event_loop_lock(zephyr_bluetooth->event_loop);
    zephyr_bluetooth->conn = bt_conn_ref(conn);
    blecon_bluetooth_on_connected(&zephyr_bluetooth->bluetooth);
    blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
}

void blecon_zephyr_bluetooth_on_disconnected(struct bt_conn* conn, uint8_t reason) {
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = _zephyr_bluetooth;
    
    blecon_event_loop_lock(zephyr_bluetooth->event_loop);

    // Ignore central connections
    if( conn != zephyr_bluetooth->conn ) {
        blecon_event_loop_unlock(zephyr_bluetooth->event_loop);
        return;
    }

    bt_conn_unref(zephyr_bluetooth->conn);
    zephyr_bluetooth->conn = NULL;
    blecon_bluetooth_on_disconnected(&zephyr_bluetooth->bluetooth);
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