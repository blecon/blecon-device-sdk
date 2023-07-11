/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"
#include "stddef.h"
#include "blecon/blecon_bearer.h"

#include "zephyr/bluetooth/l2cap.h"

struct blecon_event_loop_t;
struct bt_conn;
struct blecon_zephyr_l2cap_bearer_t {
    struct blecon_bearer_t bearer;
    struct blecon_event_loop_t* event_loop;
    bool client_nserver;
    struct bt_conn* conn;
    struct bt_l2cap_le_chan l2cap_chan;
};


void blecon_zephyr_l2cap_bearer_init_server(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer, struct blecon_event_loop_t* event_loop);
void blecon_zephyr_l2cap_bearer_server_accept(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer, struct bt_conn* conn, struct bt_l2cap_chan** chan);

void blecon_zephyr_l2cap_bearer_init_client(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer, struct blecon_event_loop_t* event_loop, struct bt_conn* conn, uint8_t psm);

struct blecon_bearer_t* blecon_zephyr_l2cap_bearer_as_bearer(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer);
void blecon_zephyr_l2cap_bearer_cleanup(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer);

static inline bool blecon_zephyr_l2cap_bearer_is_connected(struct blecon_zephyr_l2cap_bearer_t* l2cap_bearer) {
    return l2cap_bearer->conn != NULL;
}

#ifdef __cplusplus
}
#endif
