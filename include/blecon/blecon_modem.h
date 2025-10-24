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

#include "blecon/blecon_request_frame.h"
#include "blecon/blecon_defs.h"
#include "blecon/blecon_bluetooth_types.h"
#include "blecon/blecon_task_queue.h"
#include "blecon/blecon_advertising_mode.h"

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
    blecon_error_invalid_parameters, ///< Invalid parameter
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

struct blecon_modem_scan_flags_t {
    bool peer_scan : 1; ///< Enable peer scan
    bool raw_scan : 1; ///< Enable raw scan
    bool active_scan : 1; ///< Enable active scan
};

struct blecon_modem_raw_scan_report_t {
    struct blecon_bluetooth_addr_t bt_addr;
    int8_t tx_power;
    int8_t rssi;
    uint8_t sid : 4;
    bool is_scan_response : 1; ///< True if this is a scan response
    const uint8_t* adv_data;
    size_t adv_data_sz;
};

struct blecon_modem_peer_scan_report_t {
    uint8_t blecon_id[BLECON_UUID_SZ];
    bool is_announcing : 1;
    int8_t tx_power;
    int8_t rssi;
};

// Callbacks
struct blecon_modem_callbacks_t {
    void (*on_connection)(struct blecon_modem_t* modem, void* user_data);
    void (*on_disconnection)(struct blecon_modem_t* modem, void* user_data);
    void (*on_outgoing_frame_queue_has_space)(struct blecon_modem_t* modem, void* user_data);
    void (*on_incoming_frame_queue_has_data)(struct blecon_modem_t* modem, void* user_data);
    uint8_t* (*incoming_frame_data_buffer_alloc)(size_t sz, uint16_t request_id, void* user_data);
    void (*on_time_update)(struct blecon_modem_t* modem, void* user_data);
    void (*on_ping_result)(struct blecon_modem_t* modem, void* user_data);
    void (*peer_scan_report)(struct blecon_modem_t* modem, const struct blecon_modem_peer_scan_report_t* report, void* user_data);
    void (*raw_scan_report)(struct blecon_modem_t* modem, const struct blecon_modem_raw_scan_report_t* report, void* user_data);
    void (*on_scan_report)(struct blecon_modem_t* modem, void* user_data);
    void (*on_scan_complete)(struct blecon_modem_t* modem, void* user_data);
};

// Implementation
struct blecon_modem_fn_t {
    enum blecon_ret_t (*setup)(struct blecon_modem_t* modem);
    enum blecon_ret_t (*get_info)(struct blecon_modem_t* modem, struct blecon_modem_info_t* info);
    enum blecon_ret_t (*set_application_data)(struct blecon_modem_t* modem, const uint8_t* application_model_id, uint32_t application_schema_version);
    enum blecon_ret_t (*set_advertising_mode)(struct blecon_modem_t* modem, enum blecon_advertising_mode_t mode);
    enum blecon_ret_t (*announce)(struct blecon_modem_t* modem);

    enum blecon_ret_t (*connection_initiate)(struct blecon_modem_t* modem);
    enum blecon_ret_t (*connection_terminate)(struct blecon_modem_t* modem);
    
    enum blecon_ret_t (*outgoing_request_frame_queue_push)(struct blecon_modem_t* modem, const struct blecon_request_frame_t* frame);
    enum blecon_ret_t (*outgoing_request_frame_queue_space)(struct blecon_modem_t* modem, size_t* space);

    enum blecon_ret_t (*incoming_request_frame_queue_pop)(struct blecon_modem_t* modem, struct blecon_request_frame_t* frame);
    enum blecon_ret_t (*incoming_request_frame_queue_count)(struct blecon_modem_t* modem, size_t* count);
    enum blecon_ret_t (*incoming_request_frame_queue_clear)(struct blecon_modem_t* modem);
    
    enum blecon_ret_t (*get_url)(struct blecon_modem_t* modem, char* url, size_t max_sz);
    enum blecon_ret_t (*get_identity)(struct blecon_modem_t* modem, uint8_t* uuid);
    enum blecon_ret_t (*get_time)(struct blecon_modem_t* modem, bool* time_valid, uint64_t* utc_time_ms_now, uint64_t* utc_time_ms_last_updated);

    enum blecon_ret_t (*ping_perform)(struct blecon_modem_t* modem, uint32_t timeout_ms);
    enum blecon_ret_t (*ping_cancel)(struct blecon_modem_t* modem);
    enum blecon_ret_t (*ping_get_latency)(struct blecon_modem_t* modem, bool* latency_available, uint32_t* connection_latency_ms, uint32_t* round_trip_latency_ms);

    enum blecon_ret_t (*scan_start)(struct blecon_modem_t* modem, struct blecon_modem_scan_flags_t flags, uint32_t duration_ms);
    enum blecon_ret_t (*scan_stop)(struct blecon_modem_t* modem);
    enum blecon_ret_t (*scan_get_data)(struct blecon_modem_t* modem, bool* overflow);
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
struct blecon_event_loop_t* blecon_modem_get_event_loop(struct blecon_modem_t* modem);
enum blecon_ret_t blecon_modem_setup(struct blecon_modem_t* modem);
enum blecon_ret_t blecon_modem_get_info(struct blecon_modem_t* modem, struct blecon_modem_info_t* info);
enum blecon_ret_t blecon_modem_set_application_data(struct blecon_modem_t* modem, const uint8_t* application_model_id, uint32_t application_schema_version);
enum blecon_ret_t blecon_modem_set_advertising_mode(struct blecon_modem_t* modem, enum blecon_advertising_mode_t mode);
enum blecon_ret_t blecon_modem_announce(struct blecon_modem_t* modem);

enum blecon_ret_t blecon_modem_connection_initiate(struct blecon_modem_t* modem);
enum blecon_ret_t blecon_modem_connection_terminate(struct blecon_modem_t* modem);

enum blecon_ret_t blecon_modem_outgoing_request_frame_queue_push(struct blecon_modem_t* modem, const struct blecon_request_frame_t* frame);
enum blecon_ret_t blecon_modem_outgoing_request_frame_queue_space(struct blecon_modem_t* modem, size_t* space);

enum blecon_ret_t blecon_modem_incoming_request_frame_queue_pop(struct blecon_modem_t* modem, struct blecon_request_frame_t* frame);
enum blecon_ret_t blecon_modem_incoming_request_frame_queue_count(struct blecon_modem_t* modem, size_t* count);
enum blecon_ret_t blecon_modem_incoming_request_frame_queue_clear(struct blecon_modem_t* modem);

enum blecon_ret_t blecon_modem_get_url(struct blecon_modem_t* modem, char* url, size_t max_sz);
enum blecon_ret_t blecon_modem_get_identity(struct blecon_modem_t* modem, uint8_t* uuid);
enum blecon_ret_t blecon_modem_get_time(struct blecon_modem_t* modem, bool* time_valid, uint64_t* utc_time_ms_now, uint64_t* utc_time_ms_last_updated);

enum blecon_ret_t blecon_modem_ping_perform(struct blecon_modem_t* modem, uint32_t timeout_ms);
enum blecon_ret_t blecon_modem_ping_cancel(struct blecon_modem_t* modem);
enum blecon_ret_t blecon_modem_ping_get_latency(struct blecon_modem_t* modem, bool* latency_available, uint32_t* connection_latency_ms, uint32_t* round_trip_latency_ms);

enum blecon_ret_t blecon_modem_scan_start(struct blecon_modem_t* modem, struct blecon_modem_scan_flags_t flags, uint32_t duration_ms);
enum blecon_ret_t blecon_modem_scan_get_data(struct blecon_modem_t* modem, bool* overflow);
enum blecon_ret_t blecon_modem_scan_stop(struct blecon_modem_t* modem);

void blecon_modem_on_connection(struct blecon_modem_t* modem);
void blecon_modem_on_disconnection(struct blecon_modem_t* modem);
void blecon_modem_on_outgoing_frame_queue_has_space(struct blecon_modem_t* modem);
void blecon_modem_on_incoming_frame_queue_has_data(struct blecon_modem_t* modem);
uint8_t* blecon_modem_incoming_frame_data_buffer_alloc(struct blecon_modem_t* modem, uint16_t request_id, size_t sz);
void blecon_modem_on_time_update(struct blecon_modem_t* modem);
void blecon_modem_on_ping_result(struct blecon_modem_t* modem);
void blecon_modem_peer_scan_report(struct blecon_modem_t* modem, const struct blecon_modem_peer_scan_report_t* report);
void blecon_modem_raw_scan_report(struct blecon_modem_t* modem, const struct blecon_modem_raw_scan_report_t* report);
void blecon_modem_on_scan_report(struct blecon_modem_t* modem);
void blecon_modem_on_scan_complete(struct blecon_modem_t* modem);

#ifdef __cplusplus
}
#endif
