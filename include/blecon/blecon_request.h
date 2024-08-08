/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
/** @file blecon_request.h
    @brief The Blecon Request API.
*/
/** @addtogroup sdk Blecon SDK API
 * @{
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"
#include "stddef.h"

#include "blecon_defs.h"
#include "blecon_list.h"
#include "blecon_request_status_code.h"

struct blecon_request_processor_t;
struct blecon_request_t;
struct blecon_request_send_data_op_t;
struct blecon_request_receive_data_op_t;

typedef uint32_t blecon_request_id_t; ///< A request ID

/**
 * @brief Callbacks structure to populate by user for requests
 * 
 */
struct blecon_request_callbacks_t {
    /**
     * @brief Called when a request has completed and all data has been sent and received by the modem,
     * or when the request terminated with an error.
     * @param request the request instance
     */
    void (*on_closed)(struct blecon_request_t* request);

    /**
     * @brief Called when the data set in blecon_request_send_data() has been sent
     * @param send_data_op the send data operation instance
     * @param data_sent a flag indicating if the data was sent correctly
     */
    void (*on_data_sent)(struct blecon_request_send_data_op_t* send_data_op, bool data_sent);

    /**
     * @brief Allocate memory for incoming data
     * @param receive_data_op the receive data operation instance
     * @param sz the size of the buffer to allocate
     * @return a pointer to the allocated buffer
     */    
    uint8_t* (*alloc_incoming_data_buffer)(struct blecon_request_receive_data_op_t* receive_data_op, size_t sz);
    
    /**
     * @brief Called when new data is received for a request.
     * @param receive_data_op the receive data operation instance
     * @param data_received a flag indicating if data was received
     * @param data the data received
     * @param sz the size of the data received
     * @param finished a flag indicating if this is the end of the data
     */
    void (*on_data_received)(struct blecon_request_receive_data_op_t* receive_data_op, bool data_received, const uint8_t* data, size_t sz, bool finished);
};

/**
 * @brief A structure used to build a request based on its attributes
 * 
 */
struct blecon_request_parameters_t {
    bool oneway; ///< If true, the request is one-way (no response expected)
    const char* namespace; ///< The namespace of this request's event type (maximum BLECON_NAMESPACE_MAX_SZ characters, including null terminator) or NULL
    const char* method; ///< The method of this request's event type (maximum BLECON_METHOD_MAX_SZ characters, including null terminator) or NULL
    const char* request_content_type; ///< The content type of the request (e.g. application/cbor, maximum BLECON_CONTENT_TYPE_MAX_SZ characters, including null terminator) or NULL
    const char* response_content_type; ///< The expected content type of the response (e.g. application/cbor, maximum BLECON_CONTENT_TYPE_MAX_SZ characters, including null terminator) or NULL
    size_t response_mtu; ///< The maximum size of a response data frame (for a single data operation, maximum BLECON_MTU)
    const struct blecon_request_callbacks_t* callbacks; ///< The callbacks to use for this request
    void* user_data; ///< User data to pass to the callbacks
};

/**
 * @brief Initialise a request
 * 
 * @param request the request instance to initialise
 * @param parameters a struct blecon_request_parameters_t structure containing the request's parameters
 */
void blecon_request_init(struct blecon_request_t* request, const struct blecon_request_parameters_t* parameters);

/**
 * @brief Retrieve a request's parameters
 * 
 * @param request the request instance
 * @return a pointer to the request's parameters
 */
const struct blecon_request_parameters_t* blecon_request_get_parameters(struct blecon_request_t* request);

/**
 * @brief Get the status of a request
 * 
 * @param request the request instance
 * @return the status code of the request
 */
enum blecon_request_status_code_t blecon_request_get_status(struct blecon_request_t* request);

/**
 * @brief Send data for a request
 * 
 * @param op a pointer to a struct blecon_request_send_data_op_t structure which is populated on success
 * @param request a pointer to the relevant request structure
 * @param data the data to send
 * @param sz the size of the data buffer (maximum BLECON_MTU)
 * @param finished a flag indicating if this is the end of the data
 * @param user_data a pointer to user-defined data which will be associated with the operation
 * @return true if the operation was queued successfully, or false on failure
 */
bool blecon_request_send_data(
    struct blecon_request_send_data_op_t* op,
    struct blecon_request_t* request, 
    const uint8_t* data, size_t sz, bool finished,
    void* user_data);

/**
 * @brief Get the user data associated with a send data operation
 * 
 * @param op the send data operation instance
 * @return a pointer to the user data
 */
void* blecon_request_send_data_op_get_user_data(struct blecon_request_send_data_op_t* op);

/**
 * @brief Start a receiving operation
 * 
 * @param op a pointer to a struct blecon_request_receive_data_op_t structure which is populated on success
 * @param request a pointer to the relevant request structure
 * @param user_data a pointer to user-defined data which will be associated with the operation
 * @return true if the operation was queued successfully, or false on failure
 */
bool blecon_request_receive_data(
    struct blecon_request_receive_data_op_t* op,
    struct blecon_request_t* request,
    void* user_data);

/**
 * @brief Get the user data associated with a receive data operation
 * 
 * @param op the receive data operation instance
 * @return a pointer to the user data
 */
void* blecon_request_receive_data_op_get_user_data(struct blecon_request_receive_data_op_t* op);

/**
 * @brief Clean-up a request
 * 
 * @param request the request to clean-up
 */
void blecon_request_cleanup(struct blecon_request_t* request);

struct blecon_request_t {
    // Parameters
    const struct blecon_request_parameters_t* parameters;
    
    // State
    struct blecon_request_processor_t* processor;
    blecon_request_id_t id;
    enum blecon_request_status_code_t status_code;
    struct {
        bool submitted : 1;
        bool open_sent : 1;
        bool headers_sent : 1;
        bool headers_received : 1;
        bool data_sent : 1;
        bool data_received : 1;
        bool reset_sent : 1;
        bool reset_received : 1;
        bool error : 1;
    } status;

    size_t pending_receive_credits;

    struct blecon_list_t send_data_ops_list;
    struct blecon_list_t receive_data_ops_list;

    struct blecon_list_node_t requests_list_node;
};

struct blecon_request_send_data_op_t {
    struct blecon_request_t* request;
    const uint8_t* data;
    size_t sz;
    bool finished;
    void* user_data;
    struct blecon_list_node_t send_data_ops_list_node;
};

struct blecon_request_receive_data_op_t {
    struct blecon_request_t* request;
    void* user_data;
    struct blecon_list_node_t receive_data_ops_list_node;
};

#ifdef __cplusplus
}
#endif

/**
 * @}
 */
