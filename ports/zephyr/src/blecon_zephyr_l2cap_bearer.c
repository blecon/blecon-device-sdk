/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stdlib.h"
#include "string.h"
#include "assert.h"

#include "blecon_zephyr_l2cap_bearer.h"
#include "blecon/blecon_defs.h"
#include "blecon/blecon_memory.h"
#include "blecon/blecon_buffer.h"
#include "blecon/blecon_buffer_queue.h"
#include "blecon/blecon_error.h"
#include "blecon/port/blecon_event_loop.h"

#include "zephyr/bluetooth/bluetooth.h"
#include "zephyr/bluetooth/conn.h"
#include "zephyr/bluetooth/l2cap.h"

static struct blecon_buffer_t blecon_zephyr_l2cap_bearer_alloc(struct blecon_bearer_t* bearer, size_t sz, void* user_data);
static size_t blecon_zephyr_l2cap_bearer_mtu(struct blecon_bearer_t* bearer, void* user_data);
static void blecon_zephyr_l2cap_bearer_send(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf, void* user_data);
static void blecon_zephyr_l2cap_bearer_close(struct blecon_bearer_t* bearer, void* user_data);

static void blecon_zephyr_l2cap_bearer_init_common(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer, struct blecon_event_loop_t* event_loop);
static void blecon_zephyr_l2cap_bearer_connect(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer, struct bt_conn* conn);

static void blecon_zephyr_l2cap_bearer_connected(struct bt_l2cap_chan* l2cap_chan);
static void blecon_zephyr_l2cap_bearer_disconnected(struct bt_l2cap_chan* l2cap_chan);
// static struct net_buf* blecon_zephyr_l2cap_bearer_alloc_buf(struct bt_l2cap_chan* l2cap_chan);
// static struct net_buf* blecon_zephyr_l2cap_bearer_alloc_seg(struct bt_l2cap_chan* l2cap_chan);
static int blecon_zephyr_l2cap_bearer_recv(struct bt_l2cap_chan* l2cap_chan, struct net_buf* buf);
static void blecon_zephyr_l2cap_bearer_sent(struct bt_l2cap_chan* l2cap_chan);

const static struct bt_l2cap_chan_ops blecon_zephyr_l2cap_ops = {
	.connected = blecon_zephyr_l2cap_bearer_connected,
	.disconnected = blecon_zephyr_l2cap_bearer_disconnected,
	// .alloc_buf = blecon_zephyr_l2cap_bearer_alloc_buf,
	// .alloc_seg = blecon_zephyr_l2cap_bearer_alloc_seg,
    .alloc_buf = NULL,
    .alloc_seg = NULL,
	.recv = blecon_zephyr_l2cap_bearer_recv,
	.sent = blecon_zephyr_l2cap_bearer_sent,
    0
};

const static struct bt_l2cap_chan_ops blecon_zephyr_l2cap_empty_ops = { 0 };

NET_BUF_POOL_FIXED_DEFINE(l2cap_tx_pool, CONFIG_BLECON_ZEPHYR_BLUETOOTH_MAX_CONNECTIONS * BLECON_L2CAP_MAX_CONNECTIONS * BLECON_L2CAP_MAX_QUEUED_TX_BUFFERS, // Handle a single connection at a time
    BT_L2CAP_SDU_BUF_SIZE(BLECON_L2CAP_MTU), 8, NULL);

void blecon_zephyr_l2cap_bearer_init_server(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer, struct blecon_event_loop_t* event_loop) {
    blecon_zephyr_l2cap_bearer_init_common(l2cap_bearer, event_loop);

    l2cap_bearer->client_nserver = false;
    l2cap_bearer->conn = NULL;
}

void blecon_zephyr_l2cap_bearer_server_accept(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer, struct bt_conn* conn, struct bt_l2cap_chan** chan) {
    blecon_zephyr_l2cap_bearer_connect(l2cap_bearer, conn);

    *chan = &l2cap_bearer->l2cap_chan.chan;
}

void blecon_zephyr_l2cap_bearer_init_client(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer, struct blecon_event_loop_t* event_loop, struct bt_conn* conn, uint8_t psm) {
    blecon_zephyr_l2cap_bearer_init_common(l2cap_bearer, event_loop);

    l2cap_bearer->client_nserver = true;

    blecon_zephyr_l2cap_bearer_connect(l2cap_bearer, conn);

    int ret = bt_l2cap_chan_connect(conn, &l2cap_bearer->l2cap_chan.chan, psm);
    blecon_assert( ret == 0 );
}

struct blecon_bearer_t* blecon_zephyr_l2cap_bearer_as_bearer(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer) {
    return &l2cap_bearer->bearer;
}

void blecon_zephyr_l2cap_bearer_cleanup(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer) {
    if( l2cap_bearer->conn != NULL ) {
        bt_l2cap_chan_disconnect(&l2cap_bearer->l2cap_chan.chan);
        l2cap_bearer->conn = NULL;
    }

    l2cap_bearer->l2cap_chan.chan.ops = &blecon_zephyr_l2cap_empty_ops;
}

void blecon_zephyr_l2cap_bearer_init_common(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer, struct blecon_event_loop_t* event_loop) {
    const static struct blecon_bearer_fn_t bearer_fn = {
        .alloc = blecon_zephyr_l2cap_bearer_alloc,
        .mtu = blecon_zephyr_l2cap_bearer_mtu,
        .send = blecon_zephyr_l2cap_bearer_send,
        .close = blecon_zephyr_l2cap_bearer_close
    };

    blecon_bearer_set_functions(&l2cap_bearer->bearer, &bearer_fn, l2cap_bearer);
    l2cap_bearer->event_loop = event_loop;
}

void blecon_zephyr_l2cap_bearer_connect(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer, struct bt_conn* conn)
{
    l2cap_bearer->conn = conn;
    memset(&l2cap_bearer->l2cap_chan, 0, sizeof(l2cap_bearer->l2cap_chan));
    l2cap_bearer->l2cap_chan.rx.mtu = BLECON_L2CAP_MTU;
    l2cap_bearer->l2cap_chan.rx.mps = BLECON_L2CAP_MPS;
	l2cap_bearer->l2cap_chan.chan.ops = &blecon_zephyr_l2cap_ops;
}

struct blecon_buffer_t blecon_zephyr_l2cap_bearer_alloc(struct blecon_bearer_t* bearer, size_t sz, void* user_data) {
    if(sz > blecon_zephyr_l2cap_bearer_mtu(bearer, user_data)) {
        blecon_fatal_error();
    }
    return blecon_buffer_queue_alloc(sz);
}

size_t blecon_zephyr_l2cap_bearer_mtu(struct blecon_bearer_t* bearer, void* user_data) {
    struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer = (struct blecon_zephyr_l2cap_bearer_t*) user_data;

    // The MTU we return is the MPS (maximum PDU size) minus two (L2CAP header size)
    // This will ensure that any SDU sent is not fragmented across multiple PDUs
    return l2cap_bearer->l2cap_chan.tx.mps - 2;
}

void blecon_zephyr_l2cap_bearer_send(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf, void* user_data) {
    struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer = (struct blecon_zephyr_l2cap_bearer_t*) user_data;

    if( l2cap_bearer->conn == NULL ) {
        // Bearer disconnected
        blecon_buffer_free(buf);
        return;
    }
    
    struct net_buf* z_buf = net_buf_alloc(&l2cap_tx_pool, K_FOREVER);
	net_buf_reserve(z_buf, BT_L2CAP_SDU_CHAN_SEND_RESERVE);
	net_buf_add_mem(z_buf, buf.data, buf.sz);

    int ret = bt_l2cap_chan_send(&l2cap_bearer->l2cap_chan.chan, z_buf);
    blecon_assert( ret == 0 );

    blecon_buffer_free(buf);
}

void blecon_zephyr_l2cap_bearer_close(struct blecon_bearer_t* bearer, void* user_data) {
    struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer = (struct blecon_zephyr_l2cap_bearer_t*) user_data;

    if( l2cap_bearer->conn == NULL ) {
        return; // Already disconnected
    }
   
    bt_l2cap_chan_disconnect(&l2cap_bearer->l2cap_chan.chan);
}

void blecon_zephyr_l2cap_bearer_connected(struct bt_l2cap_chan* l2cap_chan) {
    struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer = (struct blecon_zephyr_l2cap_bearer_t*)
        ((char*)l2cap_chan - offsetof(struct blecon_zephyr_l2cap_bearer_t, l2cap_chan));

    blecon_event_loop_lock(l2cap_bearer->event_loop);
    blecon_bearer_on_open(&l2cap_bearer->bearer);
    blecon_event_loop_unlock(l2cap_bearer->event_loop);
}

void blecon_zephyr_l2cap_bearer_disconnected(struct bt_l2cap_chan* l2cap_chan) {
    struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer = (struct blecon_zephyr_l2cap_bearer_t*)
        ((char*)l2cap_chan - offsetof(struct blecon_zephyr_l2cap_bearer_t, l2cap_chan));

    blecon_event_loop_lock(l2cap_bearer->event_loop);
    l2cap_bearer->conn = NULL; // Indicate bearer is disconnected

    blecon_bearer_on_closed(&l2cap_bearer->bearer);
    blecon_event_loop_unlock(l2cap_bearer->event_loop);
}

int blecon_zephyr_l2cap_bearer_recv(struct bt_l2cap_chan* l2cap_chan, struct net_buf* buf) {
    struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer = (struct blecon_zephyr_l2cap_bearer_t*)
        ((char*)l2cap_chan - offsetof(struct blecon_zephyr_l2cap_bearer_t, l2cap_chan));
    
    blecon_event_loop_lock(l2cap_bearer->event_loop);
    struct blecon_buffer_t b_buf = blecon_buffer_alloc(buf->len);
    memcpy(b_buf.data, buf->data, buf->len);

    blecon_bearer_on_received(&l2cap_bearer->bearer, b_buf);
    blecon_event_loop_unlock(l2cap_bearer->event_loop);

    return 0; // buf will be dereferenced by caller
}

void blecon_zephyr_l2cap_bearer_sent(struct bt_l2cap_chan* l2cap_chan) {
    struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer = (struct blecon_zephyr_l2cap_bearer_t*)
        ((char*)l2cap_chan - offsetof(struct blecon_zephyr_l2cap_bearer_t, l2cap_chan));
    
    blecon_event_loop_lock(l2cap_bearer->event_loop);
    blecon_bearer_on_sent(&l2cap_bearer->bearer);
    blecon_event_loop_unlock(l2cap_bearer->event_loop);
}
