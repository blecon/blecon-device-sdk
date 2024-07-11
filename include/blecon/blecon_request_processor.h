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

#include "blecon_list.h"
#include "blecon_request.h"

struct blecon_request_processor_t;
struct blecon_request_t;
struct blecon_request_frame_t;

struct blecon_request_processor_client_t;
struct blecon_request_processor_client_callbacks_t {
    void (*on_connection)(struct blecon_request_processor_client_t* client);
    void (*on_disconnection)(struct blecon_request_processor_client_t* client);
};

struct blecon_request_processor_client_t {
    struct blecon_request_processor_t* processor;
    const struct blecon_request_processor_client_callbacks_t* callbacks;
    void* user_data;
    bool connection_target;
    struct blecon_list_node_t node;
};

struct blecon_request_processor_fn_t {
    bool (*set_connection_target)(struct blecon_request_processor_t* processor, bool connection_target);
    bool (*can_send_frame)(struct blecon_request_processor_t* processor);
    bool (*frame_send)(struct blecon_request_processor_t* processor, struct blecon_request_frame_t* frame);
};

struct blecon_request_processor_t {
    const struct blecon_request_processor_fn_t* fn;
    bool connected;
    size_t connection_target_count;
    struct blecon_list_t requests_list;
    blecon_request_id_t next_request_id;
    struct blecon_list_t clients_list;
};

void blecon_request_processor_init(struct blecon_request_processor_t* processor, const struct blecon_request_processor_fn_t* fn);

void blecon_request_processor_add_client(struct blecon_request_processor_t* processor, struct blecon_request_processor_client_t* client, const struct blecon_request_processor_client_callbacks_t* callbacks, void* user_data);

void blecon_request_processor_remove_client(struct blecon_request_processor_t* processor, struct blecon_request_processor_client_t* client);

bool blecon_request_processor_client_connection_initiate(struct blecon_request_processor_client_t* client);

bool blecon_request_processor_client_connection_terminate(struct blecon_request_processor_client_t* client);

bool blecon_request_processor_client_is_connected(struct blecon_request_processor_client_t* client);

void blecon_request_processor_submit(struct blecon_request_processor_t* processor, struct blecon_request_t* request);

void blecon_request_processor_process(struct blecon_request_processor_t* processor, bool disconnected);

void blecon_request_processor_receive_frame(struct blecon_request_processor_t* processor, struct blecon_request_frame_t* frame);

uint8_t* blecon_request_processor_data_buffer_alloc(struct blecon_request_processor_t* processor, size_t sz, uint16_t request_id);

struct blecon_request_processor_t* blecon_request_processor_client_get_processor(struct blecon_request_processor_client_t* client);

void* blecon_request_processor_client_get_user_data(struct blecon_request_processor_client_t* client);

#ifdef __cplusplus
}
#endif
