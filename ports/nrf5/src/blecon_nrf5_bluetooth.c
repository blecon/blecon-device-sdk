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

#define APP_BLE_CONN_CFG_TAG 1 // Softdevice configuration identifier
#define APP_BLE_OBSERVER_PRIO 3 // Priority of our BLE observer

#define DEVICE_NAME "Blecon"

#define BLE_TX_POWER                        (0)
#define MIN_CONN_INTERVAL                   MSEC_TO_UNITS(15, UNIT_1_25_MS)        // 15ms target minimum connection interval
#define MAX_CONN_INTERVAL                   MSEC_TO_UNITS(15, UNIT_1_25_MS)        // 15ms target maximum connection interval
#define SLAVE_LATENCY                       0
#define CONN_SUP_TIMEOUT                    MSEC_TO_UNITS(4000, UNIT_10_MS)        // 4 seconds connection supervisory timeout

#define FIRST_CONN_PARAMS_UPDATE_DELAY      APP_TIMER_TICKS(5000)                   // Time from initiating event (connect or start of notification) to first time connection parameters are sent (5 seconds).
#define NEXT_CONN_PARAMS_UPDATE_DELAY       APP_TIMER_TICKS(30000)                  // Time between each connection parameters update (30 seconds).
#define MAX_CONN_PARAMS_UPDATE_COUNT        3                                       // Number of attempts before giving up the negotiation.

// Validate nRF5 SDK config
#if NRF_BLE_CONN_PARAMS_ENABLED != 1
#error "BLE Connection Parameters module must be enabled in the nRF5 SDK config"
#endif

static struct blecon_nrf5_bluetooth_t _nrf5_bluetooth;

APP_TIMER_DEF(_timer_id);

static void advertising_stop(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth);
static void advertising_start(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth);
static void softdevice_init(void);
static void ble_stack_init(void);
static void gap_params_init(void);
static void conn_params_init(void);
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context);
static void adv_timeout_handler(void * p_context);

static void blecon_nrf5_bluetooth_setup(struct blecon_bluetooth_t* bluetooth);
static int32_t blecon_nrf5_bluetooth_advertising_tx_power(struct blecon_bluetooth_t* bluetooth);
static int32_t blecon_nrf5_bluetooth_connection_tx_power(struct blecon_bluetooth_t* bluetooth);
static int32_t blecon_nrf5_bluetooth_connection_rssi(struct blecon_bluetooth_t* bluetooth);
static enum blecon_bluetooth_phy_t blecon_nrf5_bluetooth_connection_phy(struct blecon_bluetooth_t* bluetooth);
static void blecon_nrf5_bluetooth_advertising_set_data(
        struct blecon_bluetooth_t* bluetooth,
        const struct blecon_bluetooth_advertising_set_t* adv_sets,
        size_t adv_sets_count
    );
static bool blecon_nrf5_bluetooth_advertising_start(struct blecon_bluetooth_t* bluetooth, uint32_t adv_interval_ms);
static void blecon_nrf5_bluetooth_advertising_stop(struct blecon_bluetooth_t* bluetooth);
static void blecon_nrf5_bluetooth_rotate_mac_address(struct blecon_bluetooth_t* bluetooth);
static void blecon_nrf5_bluetooth_get_mac_address(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_addr_t* bt_mac_addr);
static void blecon_nrf5_bluetooth_disconnect(struct blecon_bluetooth_t* bluetooth);

struct blecon_bluetooth_t* blecon_nrf5_bluetooth_init(void) {
    static const struct blecon_bluetooth_fn_t bluetooth_fn = {
        .setup = blecon_nrf5_bluetooth_setup,
        .advertising_tx_power = blecon_nrf5_bluetooth_advertising_tx_power,
        .connection_tx_power = blecon_nrf5_bluetooth_connection_tx_power,
        .connection_rssi = blecon_nrf5_bluetooth_connection_rssi,
        .connection_phy = blecon_nrf5_bluetooth_connection_phy,
        .advertising_set_data = blecon_nrf5_bluetooth_advertising_set_data,
        .advertising_start = blecon_nrf5_bluetooth_advertising_start,
        .advertising_stop = blecon_nrf5_bluetooth_advertising_stop,
        .rotate_mac_address = blecon_nrf5_bluetooth_rotate_mac_address,
        .get_mac_address = blecon_nrf5_bluetooth_get_mac_address,
        .disconnect = blecon_nrf5_bluetooth_disconnect,
        .l2cap_server_new = blecon_nrf5_bluetooth_l2cap_server_new,
        .l2cap_server_as_bearer = blecon_nrf5_bluetooth_l2cap_server_as_bearer,
        .l2cap_server_free = blecon_nrf5_bluetooth_l2cap_server_free,
        .gatts_bearer_new = blecon_nrf5_bluetooth_gatts_bearer_new,
        .gatts_bearer_as_bearer = blecon_nrf5_bluetooth_gatts_bearer_as_bearer,
        .gatts_bearer_free = blecon_nrf5_bluetooth_gatts_bearer_free
    };

    blecon_bluetooth_init(&_nrf5_bluetooth.bluetooth, &bluetooth_fn);

    _nrf5_bluetooth.conn_handle = BLE_CONN_HANDLE_INVALID;
    _nrf5_bluetooth.conn_phy = blecon_bluetooth_phy_1m;
    _nrf5_bluetooth.advertising = false;

    blecon_nrf5_bluetooth_gatts_init(&_nrf5_bluetooth);

    return &_nrf5_bluetooth.bluetooth;
}

void blecon_nrf5_bluetooth_setup(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*)bluetooth;

    // Initialize softdevice here
    softdevice_init();

    // Initialize state
    nrf5_bluetooth->advertising = false;
    nrf5_bluetooth->adv_data.handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
    nrf5_bluetooth->adv_data.sets_count = 0;
    nrf5_bluetooth->adv_data.current_set = 0;
    nrf5_bluetooth->adv_data.next_set = 0;
    
    // Initialize BLE stack
    ble_stack_init();
    gap_params_init();
    conn_params_init();

    // Initialize GATT server module
    blecon_nrf5_bluetooth_gatts_setup(nrf5_bluetooth);
    
    // Create timer
    ret_code_t err_code = app_timer_create(&_timer_id,
    APP_TIMER_MODE_REPEATED, adv_timeout_handler);
    blecon_assert(err_code == NRF_SUCCESS);

    // Retrieve base MAC address from softdevice
    sd_ble_gap_addr_get(&nrf5_bluetooth->adv_data.base_addr);
}

int32_t blecon_nrf5_bluetooth_advertising_tx_power(struct blecon_bluetooth_t* bluetooth) {
    return BLE_TX_POWER;
}

int32_t blecon_nrf5_bluetooth_connection_tx_power(struct blecon_bluetooth_t* bluetooth) {
    return BLE_TX_POWER;
}

int32_t blecon_nrf5_bluetooth_connection_rssi(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*)bluetooth;
    blecon_assert(nrf5_bluetooth->conn_handle != BLE_CONN_HANDLE_INVALID);
 
    int8_t rssi = 0;
    uint8_t ch_index = 0;
    ret_code_t err_code = sd_ble_gap_rssi_get(nrf5_bluetooth->conn_handle, &rssi, &ch_index);
    blecon_assert(err_code == NRF_SUCCESS);

    return rssi;
}

enum blecon_bluetooth_phy_t blecon_nrf5_bluetooth_connection_phy(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*)bluetooth;
    return nrf5_bluetooth->conn_phy;
}

void blecon_nrf5_bluetooth_advertising_set_data(
        struct blecon_bluetooth_t* bluetooth,
        const struct blecon_bluetooth_advertising_set_t* adv_sets,
        size_t adv_sets_count
    ) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*)bluetooth;
    // We need to save the advertising data here, and we need to alternate between buffers
    if(adv_sets_count > BLECON_NRF5_BLUETOOTH_MAX_ADV_SETS) {
        blecon_fatal_error();
    }

    // If advertising is running, stop
    if(nrf5_bluetooth->advertising) {
        advertising_stop(nrf5_bluetooth);
    }

    memcpy(&nrf5_bluetooth->adv_data.sets, adv_sets, sizeof(struct blecon_bluetooth_advertising_set_t) * adv_sets_count);
    nrf5_bluetooth->adv_data.sets_count = adv_sets_count;
    if(nrf5_bluetooth->adv_data.next_set >= nrf5_bluetooth->adv_data.sets_count) {
        nrf5_bluetooth->adv_data.next_set = 0;
    }

    if(nrf5_bluetooth->advertising) {
        advertising_start(nrf5_bluetooth);
    }
}

bool blecon_nrf5_bluetooth_advertising_start(struct blecon_bluetooth_t* bluetooth, uint32_t adv_interval_ms) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*)bluetooth;
    nrf5_bluetooth->advertising = true;
    nrf5_bluetooth->adv_data.next_set = 0;

    // Start timer
    ret_code_t err_code;
    err_code = app_timer_start(_timer_id, APP_TIMER_TICKS(adv_interval_ms), NULL);
    blecon_assert(err_code == NRF_SUCCESS);

    advertising_start(nrf5_bluetooth);
    return true;
}

void blecon_nrf5_bluetooth_advertising_stop(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*)bluetooth;
    nrf5_bluetooth->advertising = false;
    app_timer_stop(_timer_id);
    advertising_stop(nrf5_bluetooth);
}

void blecon_nrf5_bluetooth_rotate_mac_address(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*)bluetooth;

    // Change MAC address
    ble_gap_addr_t* new_address = &nrf5_bluetooth->adv_data.base_addr;
    
    struct blecon_crypto_t* crypto = blecon_nrf5_crypto_internal_get();
    
    blecon_crypto_get_random(crypto, new_address->addr, BLE_GAP_ADDR_LEN);

    new_address->addr[5] |= 0xc0; // Random static address
}

void blecon_nrf5_bluetooth_get_mac_address(struct blecon_bluetooth_t* bluetooth, struct blecon_bluetooth_addr_t* bt_mac_addr) {
    ble_gap_addr_t adv_addr;
    sd_ble_gap_addr_get(&adv_addr);
    bt_mac_addr->addr_type = adv_addr.addr_type;
    memcpy(bt_mac_addr->bytes, &adv_addr.addr[0], BLECON_BLUETOOTH_ADDR_SZ);
}

void blecon_nrf5_bluetooth_disconnect(struct blecon_bluetooth_t* bluetooth) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = (struct blecon_nrf5_bluetooth_t*)bluetooth;
    if(nrf5_bluetooth->conn_handle == BLE_CONN_HANDLE_INVALID) {
        return;
    }
    
    ret_code_t err_code = sd_ble_gap_disconnect(nrf5_bluetooth->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    blecon_assert( (err_code == 0) || (err_code == NRF_ERROR_INVALID_STATE /* if disconnection was already triggered, but hasn't completed yet */) );
}

// Private functions
void advertising_stop(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth) {
    sd_ble_gap_adv_stop(nrf5_bluetooth->adv_data.handle);
}

void advertising_start(struct blecon_nrf5_bluetooth_t* nrf5_bluetooth) {
    const struct blecon_bluetooth_advertising_set_t* adv_set = &nrf5_bluetooth->adv_data.sets[nrf5_bluetooth->adv_data.next_set];

    // Advertising parameters should only be set if advertising is not running
    ble_gap_adv_params_t adv_params = {0};
    uint32_t ret = 0;
    #if BLECON_CODED_PHY
    if( adv_set->phy == blecon_bluetooth_phy_1m ) {
    #endif
        adv_params.primary_phy     = BLE_GAP_PHY_1MBPS;
    #if BLECON_CODED_PHY
    } else {
        adv_params.primary_phy     = BLE_GAP_PHY_CODED;
    }
    #endif

    adv_params.secondary_phy = adv_params.primary_phy;
    adv_params.duration        = BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED;
    #if BLECON_CODED_PHY
    if( adv_set->phy == blecon_bluetooth_phy_1m ) {
    #endif
        if(adv_set->is_connectable) {
            adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
        } else {
            adv_params.properties.type = BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED;
        }
    #if BLECON_CODED_PHY
    } else { 
        if(adv_set->is_connectable) {
            adv_params.properties.type = BLE_GAP_ADV_TYPE_EXTENDED_CONNECTABLE_NONSCANNABLE_UNDIRECTED;
        } else {
            adv_params.properties.type = BLE_GAP_ADV_TYPE_EXTENDED_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED;
        }
    }
    #endif
    adv_params.p_peer_addr     = NULL;
    adv_params.filter_policy   = BLE_GAP_ADV_FP_ANY;
    // Use maximum advertising interval here as we're managing these manually
    adv_params.interval        = BLE_GAP_ADV_INTERVAL_MAX;

    adv_params.max_adv_evts = 1; // Always max 1 event
    // Tailgate all advertising sets
    adv_params.duration = 0;

    ble_gap_adv_data_t ble_gap_adv_data = {0};
    ble_gap_adv_data.adv_data.p_data = (uint8_t*)adv_set->adv_data;
    ble_gap_adv_data.adv_data.len = adv_set->adv_data_sz;
    
    ret = sd_ble_gap_adv_set_configure(&nrf5_bluetooth->adv_data.handle, &ble_gap_adv_data, &adv_params);
    blecon_assert(ret == NRF_SUCCESS);

    // Set TX power for this advertising handle
    ret = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, nrf5_bluetooth->adv_data.handle, BLE_TX_POWER);
    blecon_assert(ret == NRF_SUCCESS);

    // Rotate mac address
    ble_gap_addr_t mac_addr = nrf5_bluetooth->adv_data.base_addr;
    mac_addr.addr[0] += nrf5_bluetooth->adv_data.next_set & 0xff; // Rotate Address LSB

    do
    {
        ret = sd_ble_gap_addr_set(&mac_addr);
    } while (ret == NRF_ERROR_INVALID_STATE);

    // Save set number
    nrf5_bluetooth->adv_data.current_set = nrf5_bluetooth->adv_data.next_set;

    // Increment rotating advertising sets counter
    nrf5_bluetooth->adv_data.next_set = (nrf5_bluetooth->adv_data.next_set + 1) % nrf5_bluetooth->adv_data.sets_count;

    // Start advertising
    ret = sd_ble_gap_adv_start(nrf5_bluetooth->adv_data.handle, APP_BLE_CONN_CFG_TAG);
    blecon_assert(ret == NRF_SUCCESS);
}

// Initialize softdevice and configure BLE parameters
void softdevice_init(void) 
{
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
    ble_cfg.conn_cfg.conn_cfg_tag                        = APP_BLE_CONN_CFG_TAG;
    ble_cfg.conn_cfg.params.l2cap_conn_cfg.rx_mps        = BLECON_NRF5_BLUETOOTH_L2CAP_MPS;
    ble_cfg.conn_cfg.params.l2cap_conn_cfg.rx_queue_size = BLECON_NRF5_BLUETOOTH_L2CAP_RX_BUF_MAX;
    ble_cfg.conn_cfg.params.l2cap_conn_cfg.tx_mps        = BLECON_NRF5_BLUETOOTH_L2CAP_MPS;
    ble_cfg.conn_cfg.params.l2cap_conn_cfg.tx_queue_size = BLECON_NRF5_BLUETOOTH_L2CAP_TX_BUF_MAX;
    ble_cfg.conn_cfg.params.l2cap_conn_cfg.ch_count      = BLECON_NRF5_BLUETOOTH_L2CAP_SERVERS_MAX;

    err_code = sd_ble_cfg_set(BLE_CONN_CFG_L2CAP, &ble_cfg, ram_start);
    blecon_assert(err_code == NRF_SUCCESS);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    blecon_assert(err_code == NRF_SUCCESS);
}

// Initialize BLE stack handler
void ble_stack_init(void)
{
    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}

// Initialize GAP parameters
void gap_params_init(void)
{
    ret_code_t              err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    blecon_assert(err_code == NRF_SUCCESS);

    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_TAG);
    blecon_assert(err_code == NRF_SUCCESS);

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    blecon_assert(err_code == NRF_SUCCESS);
}

// Initialize preferred connection parameters
void conn_params_init(void)
{
    ret_code_t             err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = NULL;
    cp_init.error_handler                  = NULL;

    err_code = ble_conn_params_init(&cp_init);
    blecon_assert(err_code == NRF_SUCCESS);
}

// Handle BLE events
void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = &_nrf5_bluetooth;
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            nrf5_bluetooth->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

            // The connection's PHY is of the current advertising set
            nrf5_bluetooth->conn_phy = nrf5_bluetooth->adv_data.sets[nrf5_bluetooth->adv_data.current_set].phy;
            
            // Enable RSSI reporting
            ret_code_t err_code = sd_ble_gap_rssi_start(nrf5_bluetooth->conn_handle, 1, 3);
            blecon_assert(err_code == NRF_SUCCESS);

            blecon_bluetooth_on_connected(&nrf5_bluetooth->bluetooth);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            nrf5_bluetooth->conn_handle = BLE_CONN_HANDLE_INVALID;
            blecon_bluetooth_on_disconnected(&nrf5_bluetooth->bluetooth);
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            blecon_assert(err_code == NRF_SUCCESS);
        } 
        break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            blecon_assert(err_code == NRF_SUCCESS);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            blecon_assert(err_code == NRF_SUCCESS);
            break;

        case BLE_GAP_EVT_ADV_SET_TERMINATED:
            // Rotate advertising set unless this is the last set
            if(nrf5_bluetooth->advertising && (nrf5_bluetooth->adv_data.next_set != 0)) {
                // Stop - in case advertising was re-started 
                // before a pending set terminated event was processed
                advertising_stop(nrf5_bluetooth);
                advertising_start(nrf5_bluetooth);
            }
            break;
        case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
            // Handle data length update request
            {
                uint8_t data_length = p_ble_evt->evt.gap_evt.params.data_length_update_request.peer_params.max_tx_octets;
                if(data_length > NRF_SDH_BLE_GAP_DATA_LENGTH) {
                    // Use maximum supported data length
                    data_length = NRF_SDH_BLE_GAP_DATA_LENGTH;
                }

                ble_gap_data_length_params_t const dlp =
                {
                    .max_rx_octets  = data_length,
                    .max_tx_octets  = data_length,
                    .max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
                    .max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
                };

                ble_gap_data_length_limitation_t dll = {0};
                ret_code_t err_code = sd_ble_gap_data_length_update(p_ble_evt->evt.gap_evt.conn_handle, &dlp, &dll);
                blecon_assert(err_code == NRF_SUCCESS);
                break;
            }
        case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
            // Data length updated
            break;
        default:
            // No implementation needed.
            break;
    }
}

void adv_timeout_handler(void * p_context) {
    struct blecon_nrf5_bluetooth_t* nrf5_bluetooth = &_nrf5_bluetooth;
    // Rotate advertising set
    // If next set is not 0, it means we had an overrun into the next slot
    // So wait for the next timeout event
    if(nrf5_bluetooth->advertising && (nrf5_bluetooth->adv_data.next_set == 0)) {
        // Stop - in case advertising was re-started 
        // before a pending timeout event was processed
        advertising_stop(nrf5_bluetooth);
        advertising_start(nrf5_bluetooth);
    }
}