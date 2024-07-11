/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blecon_nrf5/blecon_nrf5_l2cap_server.h"
#include "blecon/blecon_error.h"
#include "nrf_assert.h"
#include "sdk_common.h"
#include "ble_srv_common.h"
#include "ble_l2cap.h"

// Table 4.15: L2CAP_LE_CREDIT_BASED_CONNECTION_REQ SPSM ranges
// Dynamic SPSM range: 0x0080 - 0x00FF

struct blecon_nrf5_l2cap_server_t {
    struct blecon_bluetooth_l2cap_server_t l2cap_server;
    struct blecon_bearer_t bearer;
    uint8_t psm;
    bool open;
    bool closing;
    // ble_l2cap_ch_rx_params_t rx_params;
    ble_l2cap_ch_tx_params_t tx_params;
    uint16_t connection_handle;
    uint16_t l2cap_cid; // Connection ID of the current connection

    struct blecon_buffer_queue_t pending_tx_bufs; // The TX buffers queued in this module
    struct blecon_buffer_queue_t tx_bufs; // TX buffers passed to the Softdevice for transmission
    struct blecon_buffer_queue_t rx_bufs; // RX buffers passed to the Softdevice for transmission
};

static struct blecon_buffer_t blecon_nrf5_l2cap_server_alloc(struct blecon_bearer_t* bearer, size_t sz, void* user_data);
static size_t blecon_nrf5_l2cap_server_mtu(struct blecon_bearer_t* bearer, void* user_data);
static void blecon_nrf5_l2cap_server_send(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf, void* user_data);
static void blecon_nrf5_l2cap_server_close(struct blecon_bearer_t* bearer, void* user_data);

static void blecon_nrf5_l2cap_server_try_send_or_close(struct blecon_nrf5_l2cap_server_t* l2cap_server);

const static struct blecon_bearer_fn_t blecon_nrf5_l2cap_server_functions = {
    .alloc = blecon_nrf5_l2cap_server_alloc,
    .mtu = blecon_nrf5_l2cap_server_mtu,
    .send = blecon_nrf5_l2cap_server_send,
    .close = blecon_nrf5_l2cap_server_close
};

static struct blecon_nrf5_l2cap_server_t _l2cap_servers[BLECON_NRF5_BLUETOOTH_L2CAP_SERVERS_MAX];
static size_t _l2cap_servers_count = 0;

static void blecon_nrf5_l2cap_server_on_ble_evt(ble_evt_t const* p_ble_evt, void* p_context);

NRF_SDH_BLE_OBSERVER(blecon_nrf5_l2cap_server_sdh_ble_obs, \
                     2 /* prio */,                 \
                     blecon_nrf5_l2cap_server_on_ble_evt, NULL /* context not used */);

struct blecon_bluetooth_l2cap_server_t* blecon_nrf5_bluetooth_l2cap_server_new(struct blecon_bluetooth_t* bluetooth, uint8_t psm) {
    ASSERT(_l2cap_servers_count < BLECON_NRF5_BLUETOOTH_L2CAP_SERVERS_MAX); // L2CAP servers are statically allocated
    struct blecon_nrf5_l2cap_server_t* nrf5_l2cap_server = &_l2cap_servers[_l2cap_servers_count];
    _l2cap_servers_count++;
    
    blecon_bearer_set_functions(&nrf5_l2cap_server->bearer, &blecon_nrf5_l2cap_server_functions, &nrf5_l2cap_server);
    nrf5_l2cap_server->psm = psm;
    nrf5_l2cap_server->connection_handle = BLE_CONN_HANDLE_INVALID;
    nrf5_l2cap_server->l2cap_cid = BLE_L2CAP_CID_INVALID;
    nrf5_l2cap_server->open = false;
    nrf5_l2cap_server->closing = false;
    blecon_buffer_queue_init(&nrf5_l2cap_server->pending_tx_bufs);
    blecon_buffer_queue_init(&nrf5_l2cap_server->tx_bufs);
    blecon_buffer_queue_init(&nrf5_l2cap_server->rx_bufs);

    return &nrf5_l2cap_server->l2cap_server;
}

struct blecon_bearer_t* blecon_nrf5_bluetooth_l2cap_server_as_bearer(struct blecon_bluetooth_l2cap_server_t* l2cap_server) {
    struct blecon_nrf5_l2cap_server_t* nrf5_l2cap_server = (struct blecon_nrf5_l2cap_server_t*) l2cap_server;
    return &nrf5_l2cap_server->bearer;
}

void blecon_nrf5_bluetooth_l2cap_server_free(struct blecon_bluetooth_l2cap_server_t* l2cap_server) {
    blecon_fatal_error(); // Dynamic creation of L2CAP servers not supported on this platform
}

struct blecon_buffer_t blecon_nrf5_l2cap_server_alloc(struct blecon_bearer_t* bearer, size_t sz, void* user_data) {
    // struct blecon_nrf5_l2cap_server_t* l2cap_server = (struct blecon_nrf5_l2cap_server_t*) ((char*)bearer - offsetof(struct blecon_nrf5_l2cap_server_t, bearer));
    if(sz > blecon_nrf5_l2cap_server_mtu(bearer, user_data)) {
        blecon_fatal_error();
    }
    return blecon_buffer_queue_alloc(sz);
}

size_t blecon_nrf5_l2cap_server_mtu(struct blecon_bearer_t* bearer, void* user_data) {
    struct blecon_nrf5_l2cap_server_t* l2cap_server = (struct blecon_nrf5_l2cap_server_t*) ((char*)bearer - offsetof(struct blecon_nrf5_l2cap_server_t, bearer));
    if( !l2cap_server->open ) {
        blecon_fatal_error();
    }

    return l2cap_server->tx_params.tx_mps - 2;
}

void blecon_nrf5_l2cap_server_send(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf, void* user_data) {
    struct blecon_nrf5_l2cap_server_t* l2cap_server = (struct blecon_nrf5_l2cap_server_t*) ((char*)bearer - offsetof(struct blecon_nrf5_l2cap_server_t, bearer));
    if(l2cap_server->open) {
        if(buf.sz > blecon_nrf5_l2cap_server_mtu(bearer, user_data)) {
            blecon_fatal_error();
        }

        // Add to queue
        blecon_buffer_queue_push(&l2cap_server->pending_tx_bufs, buf);

        blecon_nrf5_l2cap_server_try_send_or_close(l2cap_server);
    } else {
        blecon_buffer_free(buf);
    }
}

void blecon_nrf5_l2cap_server_close(struct blecon_bearer_t* bearer, void* user_data) {
    struct blecon_nrf5_l2cap_server_t* l2cap_server = (struct blecon_nrf5_l2cap_server_t*) ((char*)bearer - offsetof(struct blecon_nrf5_l2cap_server_t, bearer));
    if(l2cap_server->open) {
        l2cap_server->closing = true;
        blecon_nrf5_l2cap_server_try_send_or_close(l2cap_server);
    }
}

void blecon_nrf5_l2cap_server_try_send_or_close(struct blecon_nrf5_l2cap_server_t* l2cap_server) {
    while( !blecon_buffer_queue_is_empty(&l2cap_server->pending_tx_bufs) ) {
        if( l2cap_server->tx_params.credits == 0 ) {
            // Do not continue until we get more credits
            return;
        }
        
        bool success = false;

        // Check buffer size
        struct blecon_buffer_t buf = blecon_buffer_queue_peek(&l2cap_server->pending_tx_bufs);

        size_t credits_needed = (buf.sz + 2) / l2cap_server->tx_params.tx_mps; // ceiling((@ref ble_data_t::len + 2) / tx_mps)    
        credits_needed += ((buf.sz + 2) % l2cap_server->tx_params.tx_mps) !=0 ? 1 : 0;

        if( l2cap_server->tx_params.credits < credits_needed ) {
            // Do not continue until we get more credits
            return;
        }

        // Dequeue buffer
        blecon_buffer_queue_pop(&l2cap_server->pending_tx_bufs);

        ble_data_t obj;
        obj.p_data = (uint8_t*)buf.data;
        obj.len    = buf.sz;

        // Decrement credits
        // Decrement the variable, which stores the currently available credits, by
        // ceiling((@ref ble_data_t::len + 2) / tx_mps)
        // (basically number of PDUs needed to transfer it)
        l2cap_server->tx_params.credits -= credits_needed;

        // Add to TX queue
        blecon_buffer_queue_push(&l2cap_server->tx_bufs, buf);

        uint32_t err_code = sd_ble_l2cap_ch_tx(l2cap_server->connection_handle, l2cap_server->l2cap_cid, &obj);
        if (err_code == NRF_ERROR_RESOURCES)
        {
            goto done;
        }
        else if(err_code == NRF_ERROR_INVALID_STATE) {
            goto done;
        }
        blecon_assert(err_code == NRF_SUCCESS);

        success = true;

    done:
        if(!success) {
            // Do not free buffer here, as it's already in the queue
            // It will be freed when the bearer is closed

            // Close bearer
            blecon_nrf5_l2cap_server_close(&l2cap_server->bearer, &l2cap_server);
            return;
        }
    }

    // If closing and no more pending TX buffers, close the bearer
    if( l2cap_server->closing ) {
        if(l2cap_server->connection_handle != BLE_CONN_HANDLE_INVALID) {
            if(l2cap_server->l2cap_cid != BLE_L2CAP_CID_INVALID) {
                sd_ble_l2cap_ch_release(l2cap_server->connection_handle, l2cap_server->l2cap_cid);
            }
            sd_ble_gap_disconnect(l2cap_server->connection_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        }

        // All buffers will be freed when the channel is released
    }
}

// This function is called when receiving a L2CAP channel request from the peer.
static inline void blecon_nrf5_l2cap_server_ch_setup_request(ble_evt_t const * p_ble_evt)
{
    uint32_t err_code;
    ble_l2cap_ch_setup_params_t ch_setup_params = {{0}};

    // Find correct server based on PSM
    struct blecon_nrf5_l2cap_server_t* l2cap_server = NULL;
    for(size_t idx = 0; idx < _l2cap_servers_count; idx++) {
        if (p_ble_evt->evt.l2cap_evt.params.ch_setup_request.le_psm == _l2cap_servers[idx].psm) {
            l2cap_server = &_l2cap_servers[idx];
            break;
        }
    }

    ch_setup_params.status = BLE_L2CAP_CH_STATUS_CODE_SUCCESS;

    if (l2cap_server == NULL) 
    {
        // PSM not found
        ch_setup_params.status = BLE_L2CAP_CH_STATUS_CODE_LE_PSM_NOT_SUPPORTED;
    } else if (l2cap_server->l2cap_cid != BLE_L2CAP_CID_INVALID) {
        // Already connected
        ch_setup_params.status = BLE_L2CAP_CH_STATUS_CODE_NO_RESOURCES;
    }

    if( ch_setup_params.status != BLE_L2CAP_CH_STATUS_CODE_SUCCESS ) {
        // Refuse setup request
        uint16_t cid = p_ble_evt->evt.l2cap_evt.local_cid;
        sd_ble_l2cap_ch_setup(p_ble_evt->evt.l2cap_evt.conn_handle,
                                &cid,
                                &ch_setup_params);
        return;
    }

    // OK
    ch_setup_params.rx_params.rx_mps = BLECON_NRF5_BLUETOOTH_L2CAP_MPS;
    if (ch_setup_params.rx_params.rx_mps < BLE_L2CAP_MPS_MIN)
    {
        ch_setup_params.rx_params.rx_mps = BLE_L2CAP_MPS_MIN;
    }
    ch_setup_params.rx_params.rx_mtu = BLECON_NRF5_BLUETOOTH_L2CAP_MTU;
    if (ch_setup_params.rx_params.rx_mtu < BLE_L2CAP_MTU_MIN)
    {
        ch_setup_params.rx_params.rx_mtu = BLE_L2CAP_MTU_MIN;
    }

    // Allocate initial SDU buffer - although the documentation claims it's optional,
    // not providing it seems to lead to corruption of the first received SDU
    // (unless a transmission is performed first)
    struct blecon_buffer_t buf = blecon_buffer_queue_alloc(BLECON_NRF5_BLUETOOTH_L2CAP_MTU);
    ch_setup_params.rx_params.sdu_buf.p_data = buf.data;
    ch_setup_params.rx_params.sdu_buf.len    = buf.sz;
    
    l2cap_server->connection_handle = p_ble_evt->evt.l2cap_evt.conn_handle; // Save connection handle
    l2cap_server->l2cap_cid = p_ble_evt->evt.l2cap_evt.local_cid; // Save provided CID
    
    err_code = sd_ble_l2cap_ch_setup(l2cap_server->connection_handle,
                                     &l2cap_server->l2cap_cid,
                                     &ch_setup_params);
    if (err_code != NRF_SUCCESS)
    {
        blecon_buffer_free(buf);
        return;   
    }
    
    blecon_buffer_queue_push(&l2cap_server->rx_bufs, buf);
}

static inline void blecon_nrf5_l2cap_server_ch_rx_setup(struct blecon_nrf5_l2cap_server_t* l2cap_server) {
    while( blecon_buffer_queue_size(&l2cap_server->rx_bufs) < BLECON_NRF5_BLUETOOTH_L2CAP_RX_BUF_MAX )
    {
        // Allocate new buffer
        struct blecon_buffer_t buf = blecon_buffer_queue_alloc(BLECON_NRF5_BLUETOOTH_L2CAP_MTU);

        ble_data_t obj;
        obj.p_data = buf.data;
        obj.len    = buf.sz;

        bool success = false;

        uint32_t err_code = sd_ble_l2cap_ch_rx(l2cap_server->connection_handle, l2cap_server->l2cap_cid, &obj);
        if (err_code == NRF_ERROR_RESOURCES)
        {
            goto done;
        }
        else if(err_code == NRF_ERROR_INVALID_STATE) {
            goto done;
        }
        blecon_assert(err_code == NRF_SUCCESS);

        blecon_buffer_queue_push(&l2cap_server->rx_bufs, buf);
        success = true;

    done:
        if(!success) {
            blecon_buffer_free(buf);
            return;
        }
    }
}


// This function is called when the L2CAP channel has been setup.
static inline void blecon_nrf5_l2cap_server_ch_setup(struct blecon_nrf5_l2cap_server_t* l2cap_server, ble_evt_t const * p_ble_evt)
{
    l2cap_server->tx_params.credits = p_ble_evt->evt.l2cap_evt.params.ch_setup.tx_params.credits;
    l2cap_server->tx_params.tx_mps  = p_ble_evt->evt.l2cap_evt.params.ch_setup.tx_params.tx_mps;
    l2cap_server->tx_params.tx_mtu  = p_ble_evt->evt.l2cap_evt.params.ch_setup.tx_params.tx_mtu;

    if(l2cap_server->tx_params.tx_mps < 2) {
        l2cap_server->tx_params.tx_mps = 2; // Should never happen but a guard
    }

    // Set-up credits
    uint32_t err_code = sd_ble_l2cap_ch_flow_control(l2cap_server->connection_handle, l2cap_server->l2cap_cid,
        BLECON_NRF5_BLUETOOTH_L2CAP_RX_BUF_MAX + BLECON_NRF5_BLUETOOTH_L2CAP_RX_EXTRA_CREDITS, NULL);
    blecon_assert(err_code == NRF_SUCCESS);
    
    // Start receiving

    // Bearer open, woohoo!
    blecon_nrf5_l2cap_server_ch_rx_setup(l2cap_server); // Start receiving
    l2cap_server->open = true;
    l2cap_server->closing = false;
    blecon_bearer_on_open(&l2cap_server->bearer);
}

// L2CAP channel released
static inline void blecon_nrf5_l2cap_server_ch_released(struct blecon_nrf5_l2cap_server_t* l2cap_server, ble_evt_t const * p_ble_evt)
{    
    l2cap_server->l2cap_cid = BLE_L2CAP_CID_INVALID;

    // Clear all buffers

    // Go through all pending TX buffers and clear them all
    while(!blecon_buffer_queue_is_empty(&l2cap_server->pending_tx_bufs)) {
        blecon_buffer_free(blecon_buffer_queue_pop(&l2cap_server->pending_tx_bufs));
    }

    // TX buffers
    while(!blecon_buffer_queue_is_empty(&l2cap_server->tx_bufs)) {
        blecon_buffer_free(blecon_buffer_queue_pop(&l2cap_server->tx_bufs));
    }

    // RX buffers
    while(!blecon_buffer_queue_is_empty(&l2cap_server->rx_bufs)) {
        blecon_buffer_free(blecon_buffer_queue_pop(&l2cap_server->rx_bufs));
    }

    if(l2cap_server->open) {
        l2cap_server->open = false;
        blecon_bearer_on_closed(&l2cap_server->bearer);
    }

    blecon_nrf5_l2cap_server_close(&l2cap_server->bearer, &l2cap_server);
}

// TX callback
static inline void blecon_nrf5_l2cap_server_ch_tx(struct blecon_nrf5_l2cap_server_t* l2cap_server, ble_evt_t const * p_ble_evt)
{
    if( blecon_buffer_queue_is_empty(&l2cap_server->tx_bufs) ) {
        blecon_fatal_error();
    }

    // Free TX buffer
    struct blecon_buffer_t buf = blecon_buffer_queue_pop(&l2cap_server->tx_bufs);
    blecon_buffer_free(buf);

    // This looks like a softdevice bug - credit events are not generated, so we reclaim them here
    size_t new_credits = (p_ble_evt->evt.l2cap_evt.params.tx.sdu_buf.len + 2) / l2cap_server->tx_params.tx_mps;
    new_credits += ((p_ble_evt->evt.l2cap_evt.params.tx.sdu_buf.len + 2) % l2cap_server->tx_params.tx_mps) !=0 ? 1 : 0;
    l2cap_server->tx_params.credits += new_credits;

    // Pass to higher layer
    blecon_bearer_on_sent(&l2cap_server->bearer);

    // TODO - do we need to handle partial sends? (don't think so)
}

// RX callback
static inline void blecon_nrf5_l2cap_server_ch_rx(struct blecon_nrf5_l2cap_server_t* l2cap_server, ble_evt_t const * p_ble_evt)
{
    if( blecon_buffer_queue_is_empty(&l2cap_server->rx_bufs) ) {
        blecon_fatal_error();
    }

    // Retrieve RX buffer
    struct blecon_buffer_t buf = blecon_buffer_queue_pop(&l2cap_server->rx_bufs);
    
    if( (p_ble_evt->evt.l2cap_evt.params.rx.sdu_buf.p_data != buf.data)
        || (p_ble_evt->evt.l2cap_evt.params.rx.sdu_len > buf.sz) ) {
        blecon_fatal_error();
    }

    // Adjust size
    buf.sz = p_ble_evt->evt.l2cap_evt.params.rx.sdu_len;

    // Setup next RX packet
    blecon_nrf5_l2cap_server_ch_rx_setup(l2cap_server);

    // Pass to higher layer
    blecon_bearer_on_received(&l2cap_server->bearer, buf);
}

// Credits callback
static inline void blecon_nrf5_l2cap_server_ch_credits(struct blecon_nrf5_l2cap_server_t* l2cap_server, ble_evt_t const * p_ble_evt) {
    // Increment credits
    l2cap_server->tx_params.credits += p_ble_evt->evt.l2cap_evt.params.credit.credits;

    // Try to send the next buffer
    blecon_nrf5_l2cap_server_try_send_or_close(l2cap_server);
}

void blecon_nrf5_l2cap_server_on_ble_evt(ble_evt_t const* p_ble_evt, void* p_context) {
    if(_l2cap_servers_count == 0) {
        return;
    }

    // Handle GAP disconnection events
    if(p_ble_evt->header.evt_id == BLE_GAP_EVT_DISCONNECTED) {
        for(size_t idx = 0; idx < _l2cap_servers_count; idx++) {
            if(_l2cap_servers[idx].open) {
                blecon_bearer_on_closed(&_l2cap_servers[idx].bearer);
            }
            _l2cap_servers[idx].connection_handle = BLE_CONN_HANDLE_INVALID;
            _l2cap_servers[idx].l2cap_cid = BLE_L2CAP_CID_INVALID;
            _l2cap_servers[idx].open = false;
            _l2cap_servers[idx].closing = false;
        }
        return;
    }

    // Handle L2CAP events
    if( p_ble_evt->header.evt_id == BLE_L2CAP_EVT_CH_SETUP_REQUEST ) {
        blecon_nrf5_l2cap_server_ch_setup_request(p_ble_evt);
        return;
    }

    // // Filter out any non-L2CAP event
    switch (p_ble_evt->header.evt_id)
    {
    case BLE_L2CAP_EVT_CH_SETUP_REFUSED:
    case BLE_L2CAP_EVT_CH_SETUP:
    case BLE_L2CAP_EVT_CH_RELEASED:
    case BLE_L2CAP_EVT_CH_SDU_BUF_RELEASED:
    case BLE_L2CAP_EVT_CH_CREDIT:
    case BLE_L2CAP_EVT_CH_RX:
    case BLE_L2CAP_EVT_CH_TX:
        break;
    
    default:
        return; 
    }

    // For any other L2CAP event, retrieve server based on CID
    struct blecon_nrf5_l2cap_server_t* l2cap_server = NULL;
    for(size_t idx = 0; idx < _l2cap_servers_count; idx++) {
        if (p_ble_evt->evt.l2cap_evt.local_cid == _l2cap_servers[idx].l2cap_cid) {
            l2cap_server = &_l2cap_servers[idx];
            break;
        }
    }

    if( l2cap_server == NULL ) {
        // Unknown L2CAP CID
        return;
    }

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_L2CAP_EVT_CH_SETUP_REFUSED:
            // Source 1 (local), status 4 (not enough resources) means softdevice config needs to be adjusted
            l2cap_server->l2cap_cid = BLE_L2CAP_CID_INVALID;
            break;

        case BLE_L2CAP_EVT_CH_SETUP:
            blecon_nrf5_l2cap_server_ch_setup(l2cap_server, p_ble_evt);
            break;

        case BLE_L2CAP_EVT_CH_RELEASED:
            blecon_nrf5_l2cap_server_ch_released(l2cap_server, p_ble_evt);
            break;

        case BLE_L2CAP_EVT_CH_SDU_BUF_RELEASED:
            break;

        case BLE_L2CAP_EVT_CH_CREDIT:
            blecon_nrf5_l2cap_server_ch_credits(l2cap_server, p_ble_evt);
            break;

        case BLE_L2CAP_EVT_CH_RX:
            blecon_nrf5_l2cap_server_ch_rx(l2cap_server, p_ble_evt);
            break;

        case BLE_L2CAP_EVT_CH_TX:
            blecon_nrf5_l2cap_server_ch_tx(l2cap_server, p_ble_evt);
            break;

        default:
            // No implementation needed.
            break;
    }
}
