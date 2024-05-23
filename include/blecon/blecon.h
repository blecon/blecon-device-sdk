/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
/** @file blecon.h
    @brief The Blecon SDK API.
*/
/** @defgroup sdk Blecon SDK API
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
#include "blecon_modem.h"

struct blecon_event_loop_t;
struct blecon_bluetooth_t;
struct blecon_crypto_t;
struct blecon_nvm_t;
struct blecon_nfc_t;
struct blecon_ext_modem_transport_t;

struct blecon_t;
struct blecon_modem_t;
struct blecon_request_t;
struct blecon_request_send_data_op_t;
struct blecon_request_receive_data_op_t;

#define BLECON_REQUEST_LOCAL_STATUS_CODE_NS 0x100
#define BLECON_REQUEST_REMOTE_STATUS_CODE_NS 0x200

/**
 * @brief Request status codes
 * 
 */
enum blecon_request_status_code_t {
    blecon_request_status_ok = 0, ///< OK
    
    // Local status codes
    blecon_request_status_pending = BLECON_REQUEST_LOCAL_STATUS_CODE_NS + 0, ///< The request is pending
    blecon_request_status_timeout = BLECON_REQUEST_LOCAL_STATUS_CODE_NS + 1, ///< Request timed out
    blecon_request_status_transport_error = BLECON_REQUEST_LOCAL_STATUS_CODE_NS + 2, ///< A transport error occurred

    // Remote status codes
    blecon_request_status_handler_not_set = BLECON_REQUEST_REMOTE_STATUS_CODE_NS + 0, ///< The network's handler is not set
    blecon_request_status_handler_timeout = BLECON_REQUEST_REMOTE_STATUS_CODE_NS + 1, ///< The network's handler timed out
    blecon_request_status_handler_failed = BLECON_REQUEST_REMOTE_STATUS_CODE_NS + 2, ///< The network's handler failed
    blecon_request_status_network_not_set = BLECON_REQUEST_REMOTE_STATUS_CODE_NS + 3 ///< The device is not attached to a Blecon network
};

typedef uint32_t blecon_request_id_t; ///< A request ID

/**
 * @brief Callbacks structure to populate by user
 * 
 */
struct blecon_callbacks_t {
    /**
     * @brief Called when a blecon connection is open.
     * @param blecon the blecon instance
     */
    void (*on_connection)(struct blecon_t* blecon);

    /**
     * @brief Called when a blecon connection is closed.
     * @param blecon the blecon instance
     */
    void (*on_disconnection)(struct blecon_t* blecon);
};

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
 * @brief Create new internal modem instance
 * 
 * @param event_loop a platform-specific event loop instance
 * @param bluetooth a platform-specific bluetooth instance
 * @param crypto a platform-specific crypto instance
 * @param nvm a platform-specific nvm instance
 * @param nfc a platform-specific nfc instance
 * @param allocator a function to allocate memory
 * @return a pointer to a struct blecon_modem_t instance
 * */
struct blecon_modem_t* blecon_int_modem_create(
    struct blecon_event_loop_t* event_loop,
    struct blecon_bluetooth_t* bluetooth,
    struct blecon_crypto_t* crypto,
    struct blecon_nvm_t* nvm,
    struct blecon_nfc_t* nfc,
    void* (*allocator)(size_t)
);

/** 
 * @brief Destroy internal modem instance
 * 
 * @param modem a pointer to a struct blecon_modem_t instance created by blecon_int_modem_create()
 * @param deallocator a function to deallocate memory or NULL
 * */
void blecon_int_modem_destroy(
    struct blecon_modem_t* modem,
    void (*deallocator)(void*)
);

/** 
 * @brief Create new external modem instance
 * 
 * @param event_loop a platform-specific event loop instance
 * @param transport a platform-specific transport instance
 * @param allocator a function to allocate memory
 * @return a pointer to a struct blecon_modem_t instance
 * */
struct blecon_modem_t* blecon_ext_modem_create(
    struct blecon_event_loop_t* event_loop,
    struct blecon_ext_modem_transport_t* transport,
    void* (*allocator)(size_t)
);

/** 
 * @brief Destroy external modem instance
 * 
 * @param modem a pointer to a struct blecon_modem_t instance created by blecon_ext_modem_create()
 * @param deallocator a function to deallocate memory or NULL
 * */
void blecon_ext_modem_destroy(
    struct blecon_modem_t* modem,
    void (*deallocator)(void*)
);

/**
 * @brief Initialise Blecon
 * @param blecon the blecon instance to initialise
 * @param modem the modem instance to use
 */
void blecon_init(struct blecon_t* blecon, struct blecon_modem_t* modem);

/**
 * @brief Set-up Blecon
 * 
 * @param blecon the blecon instance to set-up
 * @return true on success, or false on failure
 */
bool blecon_setup(struct blecon_t* blecon);

/**
 * @brief Set callbacks
 * 
 * @param blecon the blecon instance
 * @param callbacks a struct blecon_modem_callbacks_t structure containing the user's callbacks
 * @param user_data user data to pass to the callbacks
 */
void blecon_set_callbacks(struct blecon_t* blecon, const struct blecon_callbacks_t* callbacks, void* user_data);

/**
 * @brief Get information about the modem
 * 
 * @param blecon the blecon instance
 * @param info a pointer to a struct blecon_modem_info_t structure which is populated on success
 */
void blecon_get_info(struct blecon_t* blecon, struct blecon_modem_info_t* info);

/**
 * @brief Set application-related data
 * 
 * @param blecon the blecon instance
 * @param application_model_id the device's specific model id encoded as a UUID (16 bytes long)
 * @param application_schema_version the device's schema version
 * @return true on success, or false on failure
 */
bool blecon_set_application_data(struct blecon_t* blecon, const uint8_t* application_model_id, uint32_t application_schema_version);

/**
 * @brief Announce the device ID to surrounding hotspots
 * 
 * @param blecon the blecon instance
 * @return true on success, or false on failure
 */
bool blecon_announce(struct blecon_t* blecon);

/**
 * @brief Initiate a connection to the Blecon infrastructure
 * 
 * @param blecon the blecon instance
 * @return true on success, or false on failure
 */
bool blecon_connection_initiate(struct blecon_t* blecon);

/**
 * @brief Terminate the current connection, or cancel the connection attempt
 * 
 * @param blecon the blecon instance
 * @return true on success, or false on failure
 */
bool blecon_connection_terminate(struct blecon_t* blecon);

/**
 * @brief Check if a connection is established
 * 
 * @param blecon the blecon instance
 * @return true if connected, or false if not
 */
bool blecon_is_connected(struct blecon_t* blecon);

/**
 * @brief Get the device's ID in URL form
 * 
 * @param blecon the blecon instance
 * @param url a char array where to store the url (0-terminated)
 * @param max_sz the maximum size of the array (including space for 0 terminator)
 * @return true on success, or false on failure
 */
bool blecon_get_url(struct blecon_t* blecon, char* url, size_t max_sz);

/**
 * @brief Get the device's ID in UUID form
 * 
 * @param blecon the blecon instance
 * @param uuid a 16-byte array where to store the uuid
 * @return true on success, or false on failure
 */
bool blecon_get_identity(struct blecon_t* blecon, uint8_t* uuid);

/**
 * @brief A structure used to build a request based on its attributes
 * 
 */
struct blecon_request_parameters_t {
    bool oneway; ///< If true, the request is one-way (no response expected)
    const char* namespace; ///< The namespace of this request's event type
    const char* method; ///< The method of this request's event type
    const char* request_content_type; ///< The content type of the request (e.g. application/cbor)
    const char* response_content_type; ///< The expected content type of the response (e.g. application/cbor)
    size_t response_mtu; ///< The maximum size of a response data frame (for a single data operation)
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
 * @brief Submit a request for processing
 * 
 * @param blecon the blecon instance
 * @param request the request to submit
 */
void blecon_request_submit(struct blecon_t* blecon, struct blecon_request_t* request);

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
 * @param sz the size of the data buffer
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

struct blecon_t {
    struct blecon_modem_t* modem;
    const struct blecon_callbacks_t* callbacks;
    void* user_data;

    bool is_setup;
    bool is_connected;
    struct blecon_modem_info_t modem_info;
    struct blecon_list_t requests_list;
    blecon_request_id_t next_request_id;
    size_t outgoing_stream_frame_queue_space_count;
};

struct blecon_request_t {
    // Parameters
    const struct blecon_request_parameters_t* parameters;
    
    // State
    struct blecon_t* blecon;
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
