/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stddef.h"

struct blecon_download_client_t;

#include "blecon/blecon_request.h"
#include "blecon/blecon_request_processor.h"
#include "blecon/blecon_list.h"

struct blecon_download_client_t;

struct blecon_download_client_receive_data_op_t {
    struct blecon_request_receive_data_op_t op;
    struct blecon_download_client_t* client;
    struct blecon_list_node_t node;
};

struct blecon_download_client_chunk_id_t {
    const void* ptr;
};

struct blecon_download_client_callbacks_t {
    void (*on_new_chunk)(struct blecon_download_client_t* client, uint8_t* data, size_t data_sz, bool done, void* user_data);
    void (*on_error)(struct blecon_download_client_t* client, void* user_data);
};

struct blecon_download_client_t {
    // Parameters
    size_t max_concurrent_receive_data_ops;

    // Callbacks
    const struct blecon_download_client_callbacks_t* callbacks;
    void* user_data;

    // State
    struct blecon_request_processor_client_t request_processor_client;
    struct blecon_request_t request;
    const char* url;
    size_t offset;
    size_t request_size;
    size_t max_size;
    bool downloading : 1;
    bool receiving_data : 1;
    bool truncated : 1;
    size_t chunks_in_use;

    // Operations
    struct blecon_list_t receive_ops;
    struct blecon_request_send_data_op_t header_send_op;
    uint8_t header_send_buffer[10];
    struct blecon_request_send_data_op_t url_send_op;
};

void blecon_download_client_init(struct blecon_download_client_t* client, 
    struct blecon_request_processor_t* request_processor,
    size_t max_concurrent_receive_data_ops,
    const struct blecon_download_client_callbacks_t* callbacks,
    void* user_data
);

void blecon_download_client_download(struct blecon_download_client_t* client, const char* url, size_t offset, size_t max_size, void* user_data);

void blecon_download_client_cancel(struct blecon_download_client_t* client);

void blecon_download_client_free_chunk(struct blecon_download_client_t* client, uint8_t* data);

#ifdef __cplusplus
}
#endif
