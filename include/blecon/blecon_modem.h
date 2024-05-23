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

#include "blecon_rpc_status_codes.h"
#include "blecon/port/blecon_event_loop.h"

struct blecon_modem_t;

/**
 * @brief Return codes
 * 
 */
enum blecon_ret_t {
    blecon_ok, ///< OK
    blecon_error_exceeds_mtu, ///< Frame exceeds MTU
    blecon_error_invalid_state, ///< This method cannot be called in the current state
    blecon_error_buffer_too_small, ///< Data doesn't fit in the buffer
    blecon_error_serialization, ///< Serialization error (external modem)
    blecon_error_transport_error, ///< Transport error (external modem)
    blecon_error_no_resource, ///< No resource available
    blecon_error_invalid_frame, ///< Invalid frame
    blecon_error_queue_full, ///< Queue is full
    blecon_error_queue_empty, ///< Queue is empty
    blecon_error_buffer_alloc_failed, ///< Buffer allocation failed
};

enum blecon_modem_info_type_t {
    blecon_modem_info_type_internal,
    blecon_modem_info_type_external,
};

/**
 * @struct Modem information
 * @brief Information about underlying modem
 * 
 */
struct blecon_modem_info_t {
    enum blecon_modem_info_type_t type; ///< The modem type (internal or external)
    uint32_t firmware_version; ///< The modem's firmware version
};

struct blecon_modem_stream_frame_open_t {
    bool flow_control_enabled;
    size_t initial_credits;
    size_t mtu;
};

struct blecon_modem_stream_outgoing_header_frame_t {
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

struct blecon_modem_stream_incoming_header_frame_t {
    enum blecon_rpc_status_code_t status_code;
};

struct blecon_modem_stream_data_frame_t {
    uint8_t* data;
    size_t data_sz;
    bool finished;
};

struct blecon_modem_stream_credit_frame_t {
    size_t credits;
};

enum blecon_modem_stream_frame_type_t {
    blecon_modem_stream_frame_type_open = 0,
    blecon_modem_stream_frame_type_outgoing_header = 1,
    blecon_modem_stream_frame_type_incoming_header = 2,
    blecon_modem_stream_frame_type_data = 3,
    blecon_modem_stream_frame_type_credit = 4,
    blecon_modem_stream_frame_type_reset = 5
};

struct blecon_modem_stream_frame_t {
    uint16_t stream_id;
    enum blecon_modem_stream_frame_type_t frame_type;
    union {
        struct blecon_modem_stream_frame_open_t open;
        struct blecon_modem_stream_outgoing_header_frame_t outgoing_header;
        struct blecon_modem_stream_incoming_header_frame_t incoming_header;
        struct blecon_modem_stream_data_frame_t data;
        struct blecon_modem_stream_credit_frame_t credit;
    } u;
};

// Callbacks
struct blecon_modem_callbacks_t {
    void (*on_connection)(struct blecon_modem_t* modem, void* user_data);
    void (*on_disconnection)(struct blecon_modem_t* modem, void* user_data);
    void (*on_outgoing_frame_queue_has_space)(struct blecon_modem_t* modem, void* user_data);
    void (*on_incoming_frame_queue_has_data)(struct blecon_modem_t* modem, void* user_data);
    uint8_t* (*incoming_frame_data_buffer_alloc)(size_t sz, uint16_t stream_id, void* user_data);
};

// Implementation
struct blecon_modem_fn_t {
    enum blecon_ret_t (*setup)(struct blecon_modem_t* modem);
    enum blecon_ret_t (*get_info)(struct blecon_modem_t* modem, struct blecon_modem_info_t* info);
    enum blecon_ret_t (*set_application_data)(struct blecon_modem_t* modem, const uint8_t* application_model_id, uint32_t application_schema_version);
    enum blecon_ret_t (*announce)(struct blecon_modem_t* modem);

    enum blecon_ret_t (*connection_initiate)(struct blecon_modem_t* modem);
    enum blecon_ret_t (*connection_terminate)(struct blecon_modem_t* modem);
    
    enum blecon_ret_t (*outgoing_stream_frame_queue_push)(struct blecon_modem_t* modem, const struct blecon_modem_stream_frame_t* frame);
    enum blecon_ret_t (*outgoing_stream_frame_queue_space)(struct blecon_modem_t* modem, size_t* space);

    enum blecon_ret_t (*incoming_stream_frame_queue_pop)(struct blecon_modem_t* modem, struct blecon_modem_stream_frame_t* frame);
    enum blecon_ret_t (*incoming_stream_frame_queue_count)(struct blecon_modem_t* modem, size_t* count);
    enum blecon_ret_t (*incoming_stream_frame_queue_clear)(struct blecon_modem_t* modem);
    
    enum blecon_ret_t (*get_url)(struct blecon_modem_t* modem, char* url, size_t max_sz);
    enum blecon_ret_t (*get_identity)(struct blecon_modem_t* modem, uint8_t* uuid);
};

struct blecon_modem_t {
    const struct blecon_modem_fn_t* fns;
    struct blecon_event_loop_t* event_loop;
    const struct blecon_modem_callbacks_t* callbacks;
    void* callbacks_user_data;
    bool is_setup;
    bool is_connected;
};

void blecon_modem_init(struct blecon_modem_t* modem, const struct blecon_modem_fn_t* fns, struct blecon_event_loop_t* event_loop);
void blecon_modem_set_callbacks(struct blecon_modem_t* modem, const struct blecon_modem_callbacks_t* callbacks, void* user_data);
bool blecon_modem_is_setup(struct blecon_modem_t* modem);
enum blecon_ret_t blecon_modem_setup(struct blecon_modem_t* modem);
enum blecon_ret_t blecon_modem_get_info(struct blecon_modem_t* modem, struct blecon_modem_info_t* info);
enum blecon_ret_t blecon_modem_set_application_data(struct blecon_modem_t* modem, const uint8_t* application_model_id, uint32_t application_schema_version);
enum blecon_ret_t blecon_modem_announce(struct blecon_modem_t* modem);

enum blecon_ret_t blecon_modem_connection_initiate(struct blecon_modem_t* modem);
enum blecon_ret_t blecon_modem_connection_terminate(struct blecon_modem_t* modem);

enum blecon_ret_t blecon_modem_outgoing_stream_frame_queue_push(struct blecon_modem_t* modem, const struct blecon_modem_stream_frame_t* frame);
enum blecon_ret_t blecon_modem_outgoing_stream_frame_queue_space(struct blecon_modem_t* modem, size_t* space);

enum blecon_ret_t blecon_modem_incoming_stream_frame_queue_pop(struct blecon_modem_t* modem, struct blecon_modem_stream_frame_t* frame);
enum blecon_ret_t blecon_modem_incoming_stream_frame_queue_count(struct blecon_modem_t* modem, size_t* count);
enum blecon_ret_t blecon_modem_incoming_stream_frame_queue_clear(struct blecon_modem_t* modem);

enum blecon_ret_t blecon_modem_get_url(struct blecon_modem_t* modem, char* url, size_t max_sz);
enum blecon_ret_t blecon_modem_get_identity(struct blecon_modem_t* modem, uint8_t* uuid);

void blecon_modem_on_connection(struct blecon_modem_t* modem);
void blecon_modem_on_disconnection(struct blecon_modem_t* modem);
void blecon_modem_on_outgoing_frame_queue_has_space(struct blecon_modem_t* modem);
void blecon_modem_on_incoming_frame_queue_has_data(struct blecon_modem_t* modem);
uint8_t* blecon_modem_incoming_frame_data_buffer_alloc(struct blecon_modem_t* modem, uint16_t stream_id, size_t sz);

#ifdef __cplusplus
}
#endif
