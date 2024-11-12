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

struct blecon_memfault_ota_client_t;

struct blecon_memfault_ota_client_callbacks_t {
    void (*on_ota_status_update)(struct blecon_memfault_ota_client_t* client, char* ota_url, void* user_data);
    void (*on_error)(struct blecon_memfault_ota_client_t* client, void* user_data);
};

struct blecon_memfault_ota_parameters_t {
    const char* hardware_version;
    const char* software_type;
    const char* software_version; // Optional
};

struct blecon_memfault_ota_client_t {
    const struct blecon_memfault_ota_parameters_t* parameters;
    const struct blecon_memfault_ota_client_callbacks_t* callbacks;
    void* user_data;
    struct blecon_request_processor_client_t request_processor_client;
    struct blecon_request_t request;
    struct blecon_request_send_data_op_t send_op;
    struct blecon_request_receive_data_op_t receive_op;
    uint8_t* send_buffer;
    bool checking_for_update : 1;
};

void blecon_memfault_ota_client_init(struct blecon_memfault_ota_client_t* client, 
    struct blecon_request_processor_t* request_processor,
    const struct blecon_memfault_ota_parameters_t* parameters,
    const struct blecon_memfault_ota_client_callbacks_t* callbacks,
    void* user_data
);

void blecon_memfault_ota_client_check_for_update(struct blecon_memfault_ota_client_t* client);

void blecon_memfault_ota_client_free_url(char* url);

#ifdef __cplusplus
}
#endif
