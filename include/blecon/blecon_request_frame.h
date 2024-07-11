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

#include "blecon_request_status_code.h"

struct blecon_request_frame_open_t {
    bool flow_control_enabled;
    size_t initial_credits;
    size_t mtu;
};

struct blecon_request_frame_outgoing_header_t {
    bool oneway;
    const char* namespace;
    size_t namespace_sz;
    const char* method;
    size_t method_sz;
    const char* request_content_type;
    size_t request_content_type_sz;
    const char* response_content_type;
    size_t response_content_type_sz;
};

struct blecon_request_frame_incoming_header_t {
    enum blecon_request_status_code_t status_code;
};

struct blecon_request_frame_data_t {
    uint8_t* data;
    size_t data_sz;
    bool finished;
};

struct blecon_request_frame_credit_t {
    size_t credits;
};

enum blecon_request_frame_type_t {
    blecon_request_frame_type_open = 0,
    blecon_request_frame_type_outgoing_header = 1,
    blecon_request_frame_type_incoming_header = 2,
    blecon_request_frame_type_data = 3,
    blecon_request_frame_type_credit = 4,
    blecon_request_frame_type_reset = 5
};

struct blecon_request_frame_t {
    uint16_t request_id;
    enum blecon_request_frame_type_t frame_type;
    union {
        struct blecon_request_frame_open_t open;
        struct blecon_request_frame_outgoing_header_t outgoing_header;
        struct blecon_request_frame_incoming_header_t incoming_header;
        struct blecon_request_frame_data_t data;
        struct blecon_request_frame_credit_t credit;
    } u;
};

#ifdef __cplusplus
}
#endif
