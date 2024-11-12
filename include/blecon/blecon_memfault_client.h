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

#include "port/blecon_memfault.h"
#include "blecon/blecon_request.h"
#include "blecon/blecon_request_processor.h"

#define BLECON_MEMFAULT_CLIENT_CONCURRENT_SEND_DATA_OPS_COUNT 2

struct blecon_memfault_client_send_data_op_t {
    struct blecon_request_send_data_op_t op;
    uint8_t* buffer;
    bool busy;
    struct blecon_memfault_client_t* client;
};

struct blecon_memfault_client_t {
    struct blecon_request_processor_client_t request_processor_client;
    struct blecon_memfault_t* memfault;
    struct blecon_request_t request;
    struct blecon_memfault_client_send_data_op_t send_ops[BLECON_MEMFAULT_CLIENT_CONCURRENT_SEND_DATA_OPS_COUNT];
    bool send_finished;
    struct blecon_request_receive_data_op_t receive_op;
    uint8_t receive_buffer[16];
};

void blecon_memfault_client_init(struct blecon_memfault_client_t* client, 
    struct blecon_request_processor_t* request_processor,
    struct blecon_memfault_t* memfault,
    const uint8_t* blecon_id
);

#ifdef __cplusplus
}
#endif
