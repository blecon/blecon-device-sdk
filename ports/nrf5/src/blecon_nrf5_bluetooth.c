/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blecon_nrf5_l2cap_server.h"
#include "blecon_nrf5_bluetooth.h"
#include "blecon_nrf5_crypto.h"
#include "blecon/blecon_bluetooth_types.h"
#include "blecon/blecon_error.h"
#include "blecon_nrf5_bluetooth_common.h"
#include "blecon_nrf5_gatts_bearer.h"

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_sdm.h"
#include "ble.h"
#include "ble_err.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
#include "nrf_ble_gatt.h"
#include "nrf_log.h"
#include "app_timer.h"

#define APP_BLE_CONN_CFG_TAG 1  // Softdevice configuration identifier
#define APP_BLE_OBSERVER_PRIO 3 // Priority of our BLE observer

#define DEVICE_NAME "Blecon"

#define BLE_TX_POWER (0)
#define MIN_CONN_INTERVAL MSEC_TO_UNITS(15, UNIT_1_25_MS) // 15ms target minimum connection interval
#define MAX_CONN_INTERVAL MSEC_TO_UNITS(15, UNIT_1_25_MS) // 15ms target maximum connection interval
#define SLAVE_LATENCY 0
#define CONN_SUP_TIMEOUT MSEC_TO_UNITS(4000, UNIT_10_MS) // 4 seconds connection supervisory timeout

#define FIRST_CONN_PARAMS_UPDATE_DELAY APP_TIMER_TICKS(5000) // Time from initiating event (connect or start of notification) to first time connection parameters are sent (5 seconds).
#define NEXT_CONN_PARAMS_UPDATE_DELAY APP_TIMER_TICKS(30000) // Time between each connection parameters update (30 seconds).
#define MAX_CONN_PARAMS_UPDATE_COUNT 3                       // Number of attempts before giving up the negotiation.

// Validate nRF5 SDK config
#if NRF_BLE_CONN_PARAMS_ENABLED != 1
#error "BLE Connection Parameters module must be enabled in the nRF5 SDK config"
#endif

// The SoftDevice observers (this file's, the GATT server bearer's and the L2CAP server's) call into
// the event loop, which only thread mode may do, and the loop never polls the SoftDevice
#if NRF_SDH_DISPATCH_MODEL != NRF_SDH_DISPATCH_MODEL_APPSH
#error "NRF_SDH_DISPATCH_MODEL must be NRF_SDH_DISPATCH_MODEL_APPSH in the nRF5 SDK config"
#endif

static struct blecon_nrf5_bluetooth_t _nrf5_bluetooth;

static size_t advertising_set_from(const struct blecon_nrf5_bluetooth_t* nrf5_bluetooth, size_t index);
static size_t advertising_set_of_buffer(const struct blecon_nrf5_bluetooth_t* nrf5_bluetooth, const uint8_t* buffer);
static void advertising_round_start(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth);
static void advertising_round_schedule(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth);
static void advertising_round_timeout(struct blecon_task_t* task, void* user_data);
static void advertising_continue(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth, size_t index);
static bool advertising_start(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth, size_t index);
static void advertising_stop(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth);
static void softdevice_init(void);
static void ble_stack_init(void);
static void gap_params_init(void);
static void conn_params_init(void);
static void ble_evt_handler(ble_evt_t const* p_ble_evt, void* p_context);

static void blecon_nrf5_bluetooth_setup(struct blecon_bluetooth_t* bluetooth);
static void blecon_nrf5_bluetooth_shutdown(struct blecon_bluetooth_t* bluetooth);
static struct blecon_bluetooth_advertising_set_t* blecon_nrf5_advertising_set_new(struct blecon_bluetooth_t* bluetooth);
static void blecon_nrf5_bluetooth_advertising_set_update_params(struct blecon_bluetooth_advertising_set_t* adv_set, struct blecon_bluetooth_advertising_params_t* params);
static void blecon_nrf5_bluetooth_advertising_set_update_data(struct blecon_bluetooth_advertising_set_t* adv_set, struct blecon_bluetooth_advertising_data_t* data);
static void blecon_nrf5_bluetooth_advertising_set_start(struct blecon_bluetooth_advertising_set_t* adv_set);
static void blecon_nrf5_bluetooth_advertising_set_stop(struct blecon_bluetooth_advertising_set_t* adv_set);
static void blecon_nrf5_bluetooth_advertising_set_free(struct blecon_bluetooth_advertising_set_t* adv_set);
static void blecon_nrf5_bluetooth_get_address(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_addr_t* bt_addr);
static void blecon_nrf5_bluetooth_connection_get_info(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_connection_info_t* info);
static void blecon_nrf5_bluetooth_connection_get_power_info(struct blecon_bluetooth_connection_t* connection, int8_t* tx_power, int8_t* rssi);
static void blecon_nrf5_bluetooth_connection_disconnect(struct blecon_bluetooth_connection_t* connection);
static void blecon_nrf5_bluetooth_connection_free(struct blecon_bluetooth_connection_t* connection);
static void blecon_nrf5_bluetooth_scan_start(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_phy_mask_t phy_mask, bool active_scan);
static void blecon_nrf5_bluetooth_scan_stop(struct blecon_bluetooth_t* bluetooth);
static void blecon_nrf5_bluetooth_setup_complete_task(struct blecon_task_t* task, void* user_data);

struct blecon_bluetooth_t* blecon_nrf5_bluetooth_init(void) {
    static const struct blecon_bluetooth_fn_t bluetooth_fn = {
        .setup = blecon_nrf5_bluetooth_setup,
        .shutdown = blecon_nrf5_bluetooth_shutdown,
        .advertising_set_new = blecon_nrf5_advertising_set_new,
        .advertising_set_update_params = blecon_nrf5_bluetooth_advertising_set_update_params,
        .advertising_set_update_data = blecon_nrf5_bluetooth_advertising_set_update_data,
        .advertising_set_start = blecon_nrf5_bluetooth_advertising_set_start,
        .advertising_set_stop = blecon_nrf5_bluetooth_advertising_set_stop,
        .advertising_set_free = blecon_nrf5_bluetooth_advertising_set_free,
        .get_address = blecon_nrf5_bluetooth_get_address,
        .l2cap_server_new = blecon_nrf5_bluetooth_l2cap_server_new,
        .gatt_server_new = blecon_nrf5_bluetooth_gatt_server_new,
        .connection_get_info = blecon_nrf5_bluetooth_connection_get_info,
        .connection_get_power_info = blecon_nrf5_bluetooth_connection_get_power_info,
        .connection_disconnect = blecon_nrf5_bluetooth_connection_disconnect,
        .connection_get_l2cap_server_bearer = blecon_nrf5_bluetooth_connection_get_l2cap_server_bearer,
        .connection_get_gatt_server_bearer = blecon_nrf5_bluetooth_connection_get_gatt_server_bearer,
        .connection_free = blecon_nrf5_bluetooth_connection_free,
        .scan_start = blecon_nrf5_bluetooth_scan_start,
        .scan_stop = blecon_nrf5_bluetooth_scan_stop
    };

    blecon_bluetooth_init(&_nrf5_bluetooth.bluetooth, &bluetooth_fn);

    _nrf5_bluetooth.advertising.sets_count = 0;
    _nrf5_bluetooth.advertising.sets_advertising = 0;
    _nrf5_bluetooth.advertising.set_current = 0;
    _nrf5_bluetooth.advertising.on_air = false;
    blecon_timeout_init(&_nrf5_bluetooth.advertising.round_timeout, advertising_round_timeout, &_nrf5_bluetooth);
    _nrf5_bluetooth.advertising.handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;

    _nrf5_bluetooth.connection.handle = BLE_CONN_HANDLE_INVALID;
    _nrf5_bluetooth.connection.phy = blecon_bluetooth_phy_1m;
    memset(&_nrf5_bluetooth.connection.our_device_id, 0, sizeof(_nrf5_bluetooth.connection.our_device_id));
    memset(&_nrf5_bluetooth.connection.peer_device_id, 0, sizeof(_nrf5_bluetooth.connection.peer_device_id));

    blecon_nrf5_bluetooth_gatt_server_init(&_nrf5_bluetooth);

#ifdef S140
    memset(&_nrf5_bluetooth.scan_params, 0, sizeof(_nrf5_bluetooth.scan_params));
    _nrf5_bluetooth.is_scanning = false;
#endif

    // Deferred setup-complete notification (raised out of setup(), never synchronously, once)
    _nrf5_bluetooth.setup_complete_raised = false;
    blecon_task_init(&_nrf5_bluetooth.setup_complete_task, blecon_nrf5_bluetooth_setup_complete_task, &_nrf5_bluetooth.bluetooth);

    return &_nrf5_bluetooth.bluetooth;
}

void blecon_nrf5_bluetooth_setup(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*) bluetooth;

    // Initialize softdevice here
    softdevice_init();

    // Initialize BLE stack
    ble_stack_init();
    gap_params_init();
    conn_params_init();

    // Initialize GATT server module
    blecon_nrf5_bluetooth_gatt_server_setup(nrf5_bluetooth);

    // The stack is ready. Raise on_setup_complete from a scheduler task rather than
    // synchronously from within setup() (control-flow rule 3: no synchronous up-calls).
    // The layers above may invoke setup() more than once on a shared base port (as the host
    // gateway's middleware stack does), so on_setup_complete is idempotent: guard against
    // double-queueing.
    if( !nrf5_bluetooth->setup_complete_raised ) {
        nrf5_bluetooth->setup_complete_raised = true;
        blecon_scheduler_queue_task(bluetooth->scheduler, &nrf5_bluetooth->setup_complete_task);
    }
}

void blecon_nrf5_bluetooth_setup_complete_task(struct blecon_task_t* task, void* user_data) {
    (void) task;
    struct blecon_bluetooth_t* bluetooth = (struct blecon_bluetooth_t*) user_data;
    blecon_bluetooth_on_setup_complete(bluetooth);
}

void blecon_nrf5_bluetooth_shutdown(struct blecon_bluetooth_t* bluetooth) {
}

struct blecon_bluetooth_advertising_set_t* blecon_nrf5_advertising_set_new(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*) bluetooth;
    blecon_assert(nrf5_bluetooth->advertising.sets_count < BLECON_MAX_ADVERTISING_SETS);

    struct blecon_nrf5_bluetooth_advertising_set_t* adv_set = &nrf5_bluetooth->advertising.sets[nrf5_bluetooth->advertising.sets_count];
    nrf5_bluetooth->advertising.sets_count++;

    memset(&adv_set->params, 0, sizeof(struct blecon_bluetooth_advertising_params_t));
    memset(&adv_set->started_params, 0, sizeof(struct blecon_bluetooth_advertising_params_t));
    memset(adv_set->data, 0, sizeof(adv_set->data));

    adv_set->data_sz = 0;
    adv_set->is_advertising = false;

    return &adv_set->set;
}

void blecon_nrf5_bluetooth_advertising_set_update_params(struct blecon_bluetooth_advertising_set_t* adv_set, struct blecon_bluetooth_advertising_params_t* params) {
    struct blecon_nrf5_bluetooth_advertising_set_t* nrf5_adv_set = (struct blecon_nrf5_bluetooth_advertising_set_t*) adv_set;

    // Update tx power
    params->tx_power = BLE_TX_POWER;

    // Save parameters
    nrf5_adv_set->params = *params;
}

void blecon_nrf5_bluetooth_advertising_set_update_data(struct blecon_bluetooth_advertising_set_t* adv_set, struct blecon_bluetooth_advertising_data_t* data) {
    struct blecon_nrf5_bluetooth_advertising_set_t* nrf5_adv_set = (struct blecon_nrf5_bluetooth_advertising_set_t*) adv_set;

    blecon_assert(data->data_sz <= sizeof(nrf5_adv_set->data));

    // Save data
    nrf5_adv_set->data_sz = data->data_sz;
    memcpy(nrf5_adv_set->data, data->data, data->data_sz);
}

void blecon_nrf5_bluetooth_advertising_set_start(struct blecon_bluetooth_advertising_set_t* adv_set) {
    struct blecon_nrf5_bluetooth_advertising_set_t* nrf5_adv_set = (struct blecon_nrf5_bluetooth_advertising_set_t*) adv_set;
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*) adv_set->bluetooth;
    if( nrf5_adv_set->is_advertising ) {
        return;
    }
    nrf5_adv_set->is_advertising = true;

    nrf5_bluetooth->advertising.sets_advertising++;
    if( nrf5_bluetooth->advertising.sets_advertising > 1 ) {
        // The rounds run: the set has its turn in this one if it comes after the set on air, else in the next
        return;
    }

    // The first set to advertise: a round now, and one every interval from now on
    advertising_round_start(nrf5_bluetooth);
}

void blecon_nrf5_bluetooth_advertising_set_stop(struct blecon_bluetooth_advertising_set_t* adv_set) {
    struct blecon_nrf5_bluetooth_advertising_set_t* nrf5_adv_set = (struct blecon_nrf5_bluetooth_advertising_set_t*) adv_set;
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*) adv_set->bluetooth;
    if( !nrf5_adv_set->is_advertising ) {
        return;
    }
    nrf5_adv_set->is_advertising = false;
    blecon_assert(nrf5_bluetooth->advertising.sets_advertising > 0);
    nrf5_bluetooth->advertising.sets_advertising--;

    size_t index = (size_t) (nrf5_adv_set - nrf5_bluetooth->advertising.sets);
    if( nrf5_bluetooth->advertising.on_air && (nrf5_bluetooth->advertising.set_current == index) ) {
        // Off the air, the round going on with the sets after it (none, if it was the last to advertise)
        advertising_stop(nrf5_bluetooth);
        advertising_continue(nrf5_bluetooth, index + 1);
    }

    if( nrf5_bluetooth->advertising.sets_advertising == 0 ) {
        // No more rounds. The set on air, if there was one, was this one
        blecon_assert(!nrf5_bluetooth->advertising.on_air);
        blecon_timeout_cancel(&nrf5_bluetooth->advertising.round_timeout);
    }
}

void blecon_nrf5_bluetooth_advertising_set_free(struct blecon_bluetooth_advertising_set_t* adv_set) {
    blecon_fatal_error(); // Not allowed
}

void blecon_nrf5_bluetooth_get_address(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_addr_t* bt_addr) {
    // Retrieve base MAC address from softdevice
    ble_gap_addr_t gap_addr = { 0 };
    ret_code_t err_code = sd_ble_gap_addr_get(&gap_addr);
    blecon_assert(err_code == NRF_SUCCESS);

    bt_addr->addr_type = gap_addr.addr_type;
    memcpy(bt_addr->bytes, gap_addr.addr, BLECON_BLUETOOTH_ADDR_SZ);
}

void blecon_nrf5_bluetooth_connection_get_info(struct blecon_bluetooth_connection_t* connection, struct blecon_bluetooth_connection_info_t* info) {
    struct blecon_nrf5_bluetooth_connection_t* nrf5_connection = (struct blecon_nrf5_bluetooth_connection_t*) connection;
    info->is_central = false; // We don't support the central role
    info->our_device_id = nrf5_connection->our_device_id;
    info->peer_device_id = nrf5_connection->peer_device_id;
    info->phy = nrf5_connection->phy;
}

void blecon_nrf5_bluetooth_connection_get_power_info(struct blecon_bluetooth_connection_t* connection, int8_t* tx_power, int8_t* rssi) {
    struct blecon_nrf5_bluetooth_connection_t* nrf5_connection = (struct blecon_nrf5_bluetooth_connection_t*) connection;
    *tx_power = BLE_TX_POWER;

    if( nrf5_connection->handle == BLE_CONN_HANDLE_INVALID ) {
        // The link is gone (its BLE_GAP_EVT_DISCONNECTED handled), and the SoftDevice may have given its handle to a
        // new link, whose RSSI it would read: no reading, and the SoftDevice is not asked
        *rssi = BLECON_BLUETOOTH_RSSI_UNKNOWN;
        return;
    }

    int8_t rssi_value = 0;
    uint8_t ch_index = 0;
    ret_code_t err_code = sd_ble_gap_rssi_get(nrf5_connection->handle, &rssi_value, &ch_index);
    if( err_code == NRF_ERROR_NOT_FOUND ) {
        // No connection event sampled yet since the reporting started at the connection: no reading
        *rssi = BLECON_BLUETOOTH_RSSI_UNKNOWN;
        return;
    }
    if( (err_code == BLE_ERROR_INVALID_CONN_HANDLE) || (err_code == NRF_ERROR_INVALID_STATE) ) {
        // The SoftDevice has ended the link and its BLE_GAP_EVT_DISCONNECTED, which reaches the port through
        // app_scheduler, is yet to be handled: the handle is refused, or the reporting has ended with the link (the port
        // starts it at the connection and never stops it), or the handle is already a new link's, whose reporting the
        // port starts only on its BLE_GAP_EVT_CONNECTED, queued behind. No reading
        *rssi = BLECON_BLUETOOTH_RSSI_UNKNOWN;
        return;
    }
    blecon_assert(err_code == NRF_SUCCESS);

    *rssi = rssi_value;
}

void blecon_nrf5_bluetooth_connection_disconnect(struct blecon_bluetooth_connection_t* connection) {
    struct blecon_nrf5_bluetooth_connection_t* nrf5_connection = (struct blecon_nrf5_bluetooth_connection_t*) connection;
    if( nrf5_connection->handle == BLE_CONN_HANDLE_INVALID ) {
        return;
    }

    ret_code_t err_code = sd_ble_gap_disconnect(nrf5_connection->handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    // NRF_ERROR_INVALID_STATE: a disconnection already under way, or the link ended; BLE_ERROR_INVALID_CONN_HANDLE: the
    // link ended. Either way its BLE_GAP_EVT_DISCONNECTED follows
    blecon_assert((err_code == NRF_SUCCESS) || (err_code == NRF_ERROR_INVALID_STATE) || (err_code == BLE_ERROR_INVALID_CONN_HANDLE));
}

void blecon_nrf5_bluetooth_connection_free(struct blecon_bluetooth_connection_t* connection) {
}

void blecon_nrf5_bluetooth_scan_start(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_phy_mask_t phy_mask, bool active_scan) {
#ifdef S140
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*) bluetooth;
    // Populate scan parameters
    nrf5_bluetooth->scan_params.extended = 1;
    nrf5_bluetooth->scan_params.report_incomplete_evts = 0;
    nrf5_bluetooth->scan_params.active = active_scan ? 1 : 0;
    nrf5_bluetooth->scan_params.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL;
    nrf5_bluetooth->scan_params.scan_phys = 0;
    if( phy_mask.phy_1m ) {
        nrf5_bluetooth->scan_params.scan_phys |= BLE_GAP_PHY_1MBPS;
    }
    if( phy_mask.phy_coded ) {
        nrf5_bluetooth->scan_params.scan_phys |= BLE_GAP_PHY_CODED;
    }
    nrf5_bluetooth->scan_params.interval = 160; // 100ms
    if( (phy_mask.phy_1m) && (phy_mask.phy_coded) ) {
        nrf5_bluetooth->scan_params.window = 80; // Share scan window between 1M and Coded PHY
    } else {
        nrf5_bluetooth->scan_params.window = 80;
    }
    nrf5_bluetooth->scan_params.timeout = BLE_GAP_SCAN_TIMEOUT_UNLIMITED;                                  // No timeout
    memset(nrf5_bluetooth->scan_params.channel_mask, 0, sizeof(nrf5_bluetooth->scan_params.channel_mask)); // Scan on all advertising channels

    ble_data_t adv_report_buffer = {
        .p_data = nrf5_bluetooth->scan_buffer,
        .len = sizeof(nrf5_bluetooth->scan_buffer)
    };

    ret_code_t err_code = sd_ble_gap_scan_start(&nrf5_bluetooth->scan_params, &adv_report_buffer);
    blecon_assert(err_code == NRF_SUCCESS);

    nrf5_bluetooth->is_scanning = true;

#else
    blecon_fatal_error(); // Not supported
#endif
}

void blecon_nrf5_bluetooth_scan_stop(struct blecon_bluetooth_t* bluetooth) {
#ifdef S140
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*) bluetooth;

    sd_ble_gap_scan_stop();

    nrf5_bluetooth->is_scanning = false;
#else
    blecon_fatal_error(); // Not supported
#endif
}

// Private functions
// The first set advertising at or after index, in index order: sets_count if none
size_t advertising_set_from(const struct blecon_nrf5_bluetooth_t* nrf5_bluetooth, size_t index) {
    while( (index < nrf5_bluetooth->advertising.sets_count) && !nrf5_bluetooth->advertising.sets[index].is_advertising ) {
        index++;
    }
    return index;
}

// The set whose data buffer the SoftDevice names in an event: sets_count if none
size_t advertising_set_of_buffer(const struct blecon_nrf5_bluetooth_t* nrf5_bluetooth, const uint8_t* buffer) {
    for( size_t index = 0; index < nrf5_bluetooth->advertising.sets_count; index++ ) {
        if( nrf5_bluetooth->advertising.sets[index].data == buffer ) {
            return index;
        }
    }
    return nrf5_bluetooth->advertising.sets_count;
}

void advertising_round_start(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth) {
    advertising_round_schedule(nrf5_bluetooth);
    advertising_continue(nrf5_bluetooth, 0);
}

// The next round's start, an interval from now
void advertising_round_schedule(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth) {
    size_t first = advertising_set_from(nrf5_bluetooth, 0);
    blecon_assert(first < nrf5_bluetooth->advertising.sets_count);

    // Every set advertises at the one interval the rounds keep
    uint32_t interval_0_625ms = nrf5_bluetooth->advertising.sets[first].params.interval_0_625ms;
    for( size_t index = first; index < nrf5_bluetooth->advertising.sets_count; index = advertising_set_from(nrf5_bluetooth, index + 1) ) {
        blecon_assert(nrf5_bluetooth->advertising.sets[index].params.interval_0_625ms == interval_0_625ms);
    }

    blecon_scheduler_queue_timeout(nrf5_bluetooth->bluetooth.scheduler, &nrf5_bluetooth->advertising.round_timeout, (interval_0_625ms * 625) / 1000);
}

void advertising_round_timeout(struct blecon_task_t* task, void* user_data) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*) user_data;
    blecon_assert(nrf5_bluetooth->advertising.sets_advertising > 0); // Cancelled when the last set stops

    if( nrf5_bluetooth->advertising.on_air ) {
        // The round before has yet to end (its set on air, or that set's end not handled yet): this round is skipped
        advertising_round_schedule(nrf5_bluetooth);
        return;
    }

    advertising_round_start(nrf5_bluetooth);
}

// Starts the first set advertising at or after index that the SoftDevice accepts; with none, the round is done and
// the next starts at the round timeout
void advertising_continue(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth, size_t index) {
    blecon_assert(!nrf5_bluetooth->advertising.on_air);
    for( size_t next = advertising_set_from(nrf5_bluetooth, index); next < nrf5_bluetooth->advertising.sets_count; next = advertising_set_from(nrf5_bluetooth, next + 1) ) {
        if( advertising_start(nrf5_bluetooth, next) ) {
            return;
        }
    }
}

// Puts the set on the SoftDevice's advertising set for one advertising event: false if the SoftDevice refuses it, a
// connectable set while the peripheral link is taken
bool advertising_start(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth, size_t index) {
    struct blecon_nrf5_bluetooth_advertising_set_t* adv_set = &nrf5_bluetooth->advertising.sets[index];

    // Advertising parameters may only be set while the SoftDevice's set is not advertising, which none is here
    ble_gap_adv_params_t adv_params = { 0 };

    // Advertising PDU
    if( adv_set->params.legacy_pdu ) {
        if( adv_set->params.is_connectable ) {
            adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
        } else {
            adv_params.properties.type = BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED;
        }
    } else {
#if S140
        if( adv_set->params.is_connectable ) {
            adv_params.properties.type = BLE_GAP_ADV_TYPE_EXTENDED_CONNECTABLE_NONSCANNABLE_UNDIRECTED;
        } else {
            adv_params.properties.type = BLE_GAP_ADV_TYPE_EXTENDED_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED;
        }
#else
        blecon_fatal_error(); // Extended advertising not supported on other softdevices
#endif
    }

    // Interval and duration
    adv_params.interval = BLE_GAP_ADV_INTERVAL_MAX;
    adv_params.duration = 0;
    adv_params.max_adv_evts = 1; // One advertising event: the round tailgates the sets, starting each as the one before ends

// PHY
#if BLECON_CODED_PHY
    if( adv_set->params.phy == blecon_bluetooth_phy_1m ) {
#endif
        adv_params.primary_phy = BLE_GAP_PHY_1MBPS;
#if BLECON_CODED_PHY
    } else {
        adv_params.primary_phy = BLE_GAP_PHY_CODED;
    }
#endif
    adv_params.secondary_phy = adv_params.primary_phy;

    // SID
    adv_params.set_id = adv_set->params.sid;

    ble_gap_adv_data_t ble_gap_adv_data = { 0 };
    ble_gap_adv_data.adv_data.p_data = (uint8_t*) adv_set->data;
    ble_gap_adv_data.adv_data.len = adv_set->data_sz;

    ret_code_t ret = sd_ble_gap_adv_set_configure(&nrf5_bluetooth->advertising.handle, &ble_gap_adv_data, &adv_params);
    blecon_assert(ret == NRF_SUCCESS);

    // Set TX power for this advertising handle
    ret = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, nrf5_bluetooth->advertising.handle, adv_set->params.tx_power);
    blecon_assert(ret == NRF_SUCCESS);

    // Set advertising address
    ble_gap_addr_t addr = { 0 };
    addr.addr_type = adv_set->params.device_id.addr.addr_type;
    memcpy(addr.addr, adv_set->params.device_id.addr.bytes, BLECON_BLUETOOTH_ADDR_SZ);

#if S140
    // If we're scanning, we need to stop doing so before being able to set the advertising address
    if( nrf5_bluetooth->is_scanning ) {
        sd_ble_gap_scan_stop();
    }
#endif

    do {
        ret = sd_ble_gap_addr_set(&addr);
    } while( ret == NRF_ERROR_INVALID_STATE );

#if S140
    // Resume scanning if needed
    if( nrf5_bluetooth->is_scanning ) {
        ble_data_t adv_report_buffer = {
            .p_data = nrf5_bluetooth->scan_buffer,
            .len = sizeof(nrf5_bluetooth->scan_buffer)
        };

        ret_code_t err_code = sd_ble_gap_scan_start(&nrf5_bluetooth->scan_params, &adv_report_buffer);
        blecon_assert(err_code == NRF_SUCCESS);
    }
#endif

    // Start advertising
    ret = sd_ble_gap_adv_start(nrf5_bluetooth->advertising.handle, APP_BLE_CONN_CFG_TAG);
    if( ret == NRF_ERROR_CONN_COUNT ) {
        // The one peripheral link the port configures is taken (its BLE_GAP_EVT_CONNECTED possibly still queued, after
        // which the client stops the advertising): no connectable set can advertise until it disconnects. This set's
        // turn is skipped
        blecon_assert(adv_set->params.is_connectable);
        return false;
    }
    blecon_assert(ret == NRF_SUCCESS);

    nrf5_bluetooth->advertising.set_current = index;
    nrf5_bluetooth->advertising.on_air = true;
    adv_set->started_params = adv_set->params;
    return true;
}

// Takes the set on air off it. The SoftDevice may have ended its advertising event already, the event (or a
// connection's) still queued
void advertising_stop(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth) {
    blecon_assert(nrf5_bluetooth->advertising.on_air);
    ret_code_t err_code = sd_ble_gap_adv_stop(nrf5_bluetooth->advertising.handle);
    blecon_assert((err_code == NRF_SUCCESS) || (err_code == NRF_ERROR_INVALID_STATE));
    nrf5_bluetooth->advertising.on_air = false;
}

// Initialize softdevice and configure BLE parameters
void softdevice_init(void) {
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    blecon_assert(err_code == NRF_SUCCESS);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    blecon_assert(err_code == NRF_SUCCESS);

    // Configure L2CAP parameters
    ble_cfg_t ble_cfg;

    memset(&ble_cfg, 0x00, sizeof(ble_cfg));
    ble_cfg.conn_cfg.conn_cfg_tag = APP_BLE_CONN_CFG_TAG;
    ble_cfg.conn_cfg.params.l2cap_conn_cfg.rx_mps = BLECON_NRF5_BLUETOOTH_L2CAP_MPS;
    ble_cfg.conn_cfg.params.l2cap_conn_cfg.rx_queue_size = BLECON_NRF5_BLUETOOTH_L2CAP_RX_BUF_MAX;
    ble_cfg.conn_cfg.params.l2cap_conn_cfg.tx_mps = BLECON_NRF5_BLUETOOTH_L2CAP_MPS;
    ble_cfg.conn_cfg.params.l2cap_conn_cfg.tx_queue_size = BLECON_NRF5_BLUETOOTH_L2CAP_TX_BUF_MAX;
    ble_cfg.conn_cfg.params.l2cap_conn_cfg.ch_count = BLECON_NRF5_BLUETOOTH_L2CAP_SERVERS_MAX;

    err_code = sd_ble_cfg_set(BLE_CONN_CFG_L2CAP, &ble_cfg, ram_start);
    blecon_assert(err_code == NRF_SUCCESS);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    blecon_assert(err_code == NRF_SUCCESS);
}

// Initialize BLE stack handler
void ble_stack_init(void) {
    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}

// Initialize GAP parameters
void gap_params_init(void) {
    ret_code_t err_code;
    ble_gap_conn_params_t gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t*) DEVICE_NAME, strlen(DEVICE_NAME));
    blecon_assert(err_code == NRF_SUCCESS);

    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_TAG);
    blecon_assert(err_code == NRF_SUCCESS);

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    blecon_assert(err_code == NRF_SUCCESS);
}

// Initialize preferred connection parameters
void conn_params_init(void) {
    ret_code_t err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail = false;
    cp_init.evt_handler = NULL;
    cp_init.error_handler = NULL;

    err_code = ble_conn_params_init(&cp_init);
    blecon_assert(err_code == NRF_SUCCESS);
}

// Handle BLE events
void ble_evt_handler(ble_evt_t const* p_ble_evt, void* p_context) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = &_nrf5_bluetooth;
    ret_code_t err_code = NRF_SUCCESS;
    switch( p_ble_evt->header.evt_id ) {
        case BLE_GAP_EVT_CONNECTED: {
            // The set the connection was made on, which the SoftDevice names by its data buffer: the port may have
            // started others since, or stopped and updated this one, before handling this event
            blecon_assert(p_ble_evt->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_PERIPH); // The port has no central role
            size_t adv_set_index = advertising_set_of_buffer(nrf5_bluetooth, p_ble_evt->evt.gap_evt.params.connected.adv_data.adv_data.p_data);
            blecon_assert(adv_set_index < nrf5_bluetooth->advertising.sets_count);
            const struct blecon_nrf5_bluetooth_advertising_set_t* adv_set = &nrf5_bluetooth->advertising.sets[adv_set_index];
            if( nrf5_bluetooth->advertising.on_air && (nrf5_bluetooth->advertising.set_current == adv_set_index) ) {
                // Its advertising has ended with the connection, which raises no BLE_GAP_EVT_ADV_SET_TERMINATED: the
                // round ends here. Stopped all the same, as on that event: should this event be of an earlier start of
                // the set, the port having stopped and started it again since, the set is on the air
                advertising_stop(nrf5_bluetooth);
            }

            // Init connection
            blecon_bluetooth_connection_init(&nrf5_bluetooth->connection.connection, &nrf5_bluetooth->bluetooth);

            nrf5_bluetooth->connection.handle = p_ble_evt->evt.gap_evt.conn_handle;

            // The connection's PHY and our address are those the set was started with
            nrf5_bluetooth->connection.phy = adv_set->started_params.phy;
            nrf5_bluetooth->connection.our_device_id = adv_set->started_params.device_id;
            struct blecon_bluetooth_addr_t peer_addr = {
                .addr_type = p_ble_evt->evt.gap_evt.params.connected.peer_addr.addr_type,
            };
            memcpy(peer_addr.bytes, p_ble_evt->evt.gap_evt.params.connected.peer_addr.addr, BLECON_BLUETOOTH_ADDR_SZ);
            nrf5_bluetooth->connection.peer_device_id = blecon_bluetooth_device_id_from_addr(peer_addr);

            // Enable RSSI reporting. BLE_ERROR_INVALID_CONN_HANDLE: the SoftDevice has ended the link since raising this
            // event (one failing to be established ends within six connection intervals), and its
            // BLE_GAP_EVT_DISCONNECTED follows. NRF_ERROR_INVALID_STATE, reporting already ongoing: the port starts it
            // once per connection, so the handle is already a new link's, which an earlier BLE_GAP_EVT_CONNECTED's start
            // reached. The connection is raised all the same
            err_code = sd_ble_gap_rssi_start(nrf5_bluetooth->connection.handle, 1, 3);
            blecon_assert((err_code == NRF_SUCCESS) || (err_code == BLE_ERROR_INVALID_CONN_HANDLE) || (err_code == NRF_ERROR_INVALID_STATE));

            blecon_bluetooth_on_new_connection(&nrf5_bluetooth->bluetooth, &nrf5_bluetooth->connection.connection, &nrf5_bluetooth->advertising.sets[adv_set_index].set);
        } break;

        case BLE_GAP_EVT_DISCONNECTED:
            nrf5_bluetooth->connection.handle = BLE_CONN_HANDLE_INVALID;
            blecon_bluetooth_connection_on_disconnected(&nrf5_bluetooth->connection.connection);
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
            ble_gap_phys_t const phys = {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            // BLE_ERROR_INVALID_CONN_HANDLE or NRF_ERROR_INVALID_STATE (no link): the SoftDevice has ended the link since
            // raising this event, and its BLE_GAP_EVT_DISCONNECTED follows
            blecon_assert((err_code == NRF_SUCCESS) || (err_code == BLE_ERROR_INVALID_CONN_HANDLE) || (err_code == NRF_ERROR_INVALID_STATE));
        } break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event. NRF_ERROR_INVALID_STATE (a disconnection under way, or no link) or
            // BLE_ERROR_INVALID_CONN_HANDLE: the link is ending or has ended, and its BLE_GAP_EVT_DISCONNECTED follows
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            blecon_assert((err_code == NRF_SUCCESS) || (err_code == NRF_ERROR_INVALID_STATE) || (err_code == BLE_ERROR_INVALID_CONN_HANDLE));
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event, the same codes accepted
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            blecon_assert((err_code == NRF_SUCCESS) || (err_code == NRF_ERROR_INVALID_STATE) || (err_code == BLE_ERROR_INVALID_CONN_HANDLE));
            break;

        case BLE_GAP_EVT_ADV_SET_TERMINATED:
            // The end of the set on air's advertising event, and the round goes on with the sets after it. An event
            // naming another set's buffer, or none on air, is of a set the port has stopped since (the stop and the
            // SoftDevice's end crossing): ignored
            if( nrf5_bluetooth->advertising.on_air
                && (p_ble_evt->evt.gap_evt.params.adv_set_terminated.adv_data.adv_data.p_data == nrf5_bluetooth->advertising.sets[nrf5_bluetooth->advertising.set_current].data) ) {
                // The set is stopped all the same: if this event is of an earlier start of it, the port having stopped
                // and started it again since, it is on air, and the SoftDevice would refuse the next set's
                // configuration. That start then loses its advertising event
                advertising_stop(nrf5_bluetooth);
                advertising_continue(nrf5_bluetooth, nrf5_bluetooth->advertising.set_current + 1);
            }
            break;
        case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
            // Handle data length update request
            {
                uint8_t data_length = p_ble_evt->evt.gap_evt.params.data_length_update_request.peer_params.max_tx_octets;
                if( data_length > NRF_SDH_BLE_GAP_DATA_LENGTH ) {
                    // Use maximum supported data length
                    data_length = NRF_SDH_BLE_GAP_DATA_LENGTH;
                }

                ble_gap_data_length_params_t const dlp = {
                    .max_rx_octets = data_length,
                    .max_tx_octets = data_length,
                    .max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
                    .max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
                };

                ble_gap_data_length_limitation_t dll = { 0 };
                err_code = sd_ble_gap_data_length_update(p_ble_evt->evt.gap_evt.conn_handle, &dlp, &dll);
                // BLE_ERROR_INVALID_CONN_HANDLE or NRF_ERROR_INVALID_STATE (no link): the SoftDevice has ended the link
                // since raising this event, and its BLE_GAP_EVT_DISCONNECTED follows
                blecon_assert((err_code == NRF_SUCCESS) || (err_code == BLE_ERROR_INVALID_CONN_HANDLE) || (err_code == NRF_ERROR_INVALID_STATE));
                break;
            }
        case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
            // Data length updated
            break;
#ifdef S140
        case BLE_GAP_EVT_ADV_REPORT: {
            if( !nrf5_bluetooth->is_scanning ) {
                // A report queued before the scan was stopped: dropped, and the scan stays stopped
                break;
            }

            if( p_ble_evt->evt.gap_evt.params.adv_report.type.status == BLE_GAP_ADV_DATA_STATUS_COMPLETE ) {
                // Ignore incomplete or truncated data

                ble_gap_evt_adv_report_t const* p_adv_report = &p_ble_evt->evt.gap_evt.params.adv_report;
                struct blecon_bluetooth_addr_t adv_addr = {
                    .addr_type = p_adv_report->peer_addr.addr_type,
                };
                memcpy(adv_addr.bytes, p_adv_report->peer_addr.addr, BLECON_BLUETOOTH_ADDR_SZ);
                struct blecon_bluetooth_advertising_info_t adv_info = {
                    .device_id = blecon_bluetooth_device_id_from_addr(adv_addr),
                    .legacy_pdu = !p_adv_report->type.extended_pdu,
                    .is_connectable = p_adv_report->type.connectable,
                    .is_scan_response = p_adv_report->type.scan_response,
                    .sid = p_adv_report->type.extended_pdu ? p_adv_report->set_id : 0,
                    .tx_power = p_adv_report->tx_power,
                    .rssi = p_adv_report->rssi,
                    .phy = p_adv_report->primary_phy == BLE_GAP_PHY_1MBPS ? blecon_bluetooth_phy_1m : blecon_bluetooth_phy_coded,
                };

                struct blecon_bluetooth_advertising_data_t adv_data = {
                    .data = p_adv_report->data.p_data,
                    .data_sz = p_adv_report->data.len,
                };
                blecon_bluetooth_on_advertising_report(&nrf5_bluetooth->bluetooth, &adv_info, &adv_data);
            }

            // Continue scanning
            ble_data_t adv_report_buffer = {
                .p_data = nrf5_bluetooth->scan_buffer,
                .len = sizeof(nrf5_bluetooth->scan_buffer)
            };
            sd_ble_gap_scan_start(NULL, &adv_report_buffer);
        } break;
#endif
        default:
            // No implementation needed.
            break;
    }
}
