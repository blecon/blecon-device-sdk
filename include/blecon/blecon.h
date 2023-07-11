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

#include "blecon_defs.h"
#include "port/blecon_event_loop.h"
#include "port/blecon_bluetooth.h"
#include "port/blecon_nvm.h"
#include "port/blecon_crypto.h"
#include "port/blecon_nfc.h"
#include "port/blecon_ext_modem_transport.h"

struct blecon_modem_t;

/**
 * @brief Return codes
 * 
 */
enum blecon_ret_t {
    blecon_ok, ///< OK
    blecon_error_exceeds_mtu, ///< Message exceeds MTU
    blecon_error_invalid_state, ///< This method cannot be called in the current state
    blecon_error_buffer_too_small, ///< Data doesn't fit in the buffer
    blecon_error_serialization, ///< Serialization error (external modem)
    blecon_error_transport_error, ///< Transport error (external modem)
};

/**
 * @brief Modem modes
 * 
 */
enum blecon_mode_t {
    blecon_mode_high_performance, ///< High Performance mode
    blecon_mode_balanced, ///< Balanced mode
    blecon_mode_ultra_low_power, ///< Low Power mode
};

#define BLECON_REQUEST_LOCAL_ERROR_CODE_NS 0x100
#define BLECON_REQUEST_REMOTE_ERROR_CODE_NS 0x200

/**
 * @brief Request errors
 * 
 */
enum blecon_request_error_t {
    blecon_request_ok = 0, ///< OK
    
    // Local error codes
    blecon_request_error_timeout = BLECON_REQUEST_LOCAL_ERROR_CODE_NS + 0, ///< Request timed out
    blecon_request_error_security_error = BLECON_REQUEST_LOCAL_ERROR_CODE_NS + 1, ///< The connection failed due to a security error
    blecon_request_error_disconnected = BLECON_REQUEST_LOCAL_ERROR_CODE_NS + 2, ///< The link was disconnected before the request completed
    
    // Remote error codes
    blecon_request_error_handler_not_set = BLECON_REQUEST_REMOTE_ERROR_CODE_NS + 0, ///< The network's handler is not set
    blecon_request_error_handler_timeout = BLECON_REQUEST_REMOTE_ERROR_CODE_NS + 1, ///< The network's handler timed out
    blecon_request_error_handler_failed = BLECON_REQUEST_REMOTE_ERROR_CODE_NS + 2, ///< The network's handler failed
    blecon_request_error_network_not_set = BLECON_REQUEST_REMOTE_ERROR_CODE_NS + 3 ///< The device is attached to a Blecon network
};

enum blecon_modem_info_type_t {
    blecon_modem_info_type_internal,
    blecon_modem_info_type_external,
};

struct blecon_modem_info_t {
    enum blecon_modem_info_type_t type;
    uint32_t firmware_version;
};

/**
 * @brief Callbacks structure to populate by user
 * 
 */
struct blecon_modem_callbacks_t {
    /**
     * @brief Called when a connection has been established.
     * 
     */
    void (*on_connection)(struct blecon_modem_t* modem, void* user_data);
    
    /**
     * @brief Called when a response is available.
     * 
     */
    void (*on_response)(struct blecon_modem_t* modem, void* user_data);
    
    /**
     * @brief Called when a request didn't complete successfully.
     * 
     */
    void (*on_error)(struct blecon_modem_t* modem, void* user_data);
};

/** 
 * @ brief Create new internal modem instance
 * 
 * @return a pointer to a struct blecon_modem_t instance
 * */
struct blecon_modem_t* blecon_int_modem_new(
        struct blecon_event_loop_t* event_loop,
        struct blecon_bluetooth_t* bluetooth,
        struct blecon_crypto_t* crypto,
        struct blecon_nvm_t* nvm,
        struct blecon_nfc_t* nfc
    );

/** 
 * @ brief Create new external modem instance
 * 
 * @return a pointer to a struct blecon_modem_t instance
 * */
struct blecon_modem_t* blecon_ext_modem_new(
    struct blecon_event_loop_t* event_loop,
    struct blecon_ext_modem_transport_t* transport);

/**
 * @brief Set-up modem
 * 
 * @param modem the modem instance to set-up
 * @return enum blecon_ret_t blecon_ok on success, or an error code on failure
 */
enum blecon_ret_t blecon_setup(struct blecon_modem_t* modem);

/**
 * @brief Set callbacks
 * 
 * @param modem the modem instance
 * @param callbacks a struct blecon_modem_callbacks_t structure containing the user's callbacks
 */
void blecon_set_callbacks(struct blecon_modem_t* modem, const struct blecon_modem_callbacks_t* callbacks, void* user_data);

/**
 * @brief Set callbacks
 * 
 * @param modem the modem instance
 * @param info a pointer to a struct blecon_modem_info_t structure which is populated on success
 * @return enum blecon_ret_t blecon_ok on success, or an error code on failure
 */
enum blecon_ret_t blecon_get_info(struct blecon_modem_t* modem, struct blecon_modem_info_t* info);

/**
 * @brief Set application-related data
 * 
 * @param modem the modem instance
 * @param application_model_id the device's specific model id encoded as a UUID (16 bytes long)
 * @param application_schema_version the device's schema version
 * @return enum blecon_ret_t blecon_ok on success, or an error code on failure
 */
enum blecon_ret_t blecon_set_application_data(struct blecon_modem_t* blecon_modem, const uint8_t* application_model_id, uint32_t application_schema_version);

/**
 * @brief Identify the device to surrounding hotspots
 * 
 * @param modem the modem instance
 * @return enum blecon_ret_t blecon_ok on success, or an error code on failure
 */
enum blecon_ret_t blecon_identify(struct blecon_modem_t* blecon_modem);

/**
 * @brief Request a connection to the Blecon infrastructure
 * 
 * @param modem the modem instance
 * @return enum blecon_ret_t blecon_ok on success, or an error code on failure
 */
enum blecon_ret_t blecon_request_connection(struct blecon_modem_t* blecon_modem);

/**
 * @brief Cancel or close the current connection
 * 
 * @param modem the modem instance
 * @return enum blecon_ret_t blecon_ok on success, or an error code on failure
 */
enum blecon_ret_t blecon_close_connection(struct blecon_modem_t* blecon_modem);

/**
 * @brief Send a request to the integration
 * 
 * @param modem the modem instance
 * @param data the data to send to the integration
 * @param sz the size of the data buffer
 * @return enum blecon_ret_t blecon_ok on success, or an error code on failure
 */
enum blecon_ret_t blecon_send_request(struct blecon_modem_t* blecon_modem, const uint8_t* data, size_t sz);

/**
 * @brief Get the error regarding the last request
 * 
 * @param modem the modem instance
 * @param error a pointer where the error code can be stored
 * @return enum blecon_ret_t blecon_ok on success, or an error code on failure
 */
enum blecon_ret_t blecon_get_error(struct blecon_modem_t* blecon_modem, enum blecon_request_error_t* request_error);

/**
 * @brief Get the response to the sent request
 * 
 * @param modem the modem instance
 * @param data where to store the data sent by the integration
 * @param sz pointer to the maximum size that can be stored in data, updated to the actual stored size on success
 * @return enum blecon_ret_t blecon_ok on success, or an error code on failure
 */
enum blecon_ret_t blecon_get_response(struct blecon_modem_t* blecon_modem, uint8_t* data, size_t* sz);

/**
 * @brief Get the device's URL
 * 
 * @param modem the modem instance
 * @param url a char array where to store the url (0-terminated)
 * @param max_sz the maximum size of the array (including space for 0 terminator)
 * @return enum blecon_ret_t blecon_ok on success, or an error code on failure
 */
enum blecon_ret_t blecon_get_url(struct blecon_modem_t* blecon_modem, char* url, size_t max_sz);

/**
 * @brief Get the device's ID in UUID form
 * 
 * @param modem the modem instance
 * @param uuid a 16-byte array where to store the uuid
 * @return enum blecon_ret_t blecon_ok on success, or an error code on failure
 */
enum blecon_ret_t blecon_get_identity(struct blecon_modem_t* blecon_modem, uint8_t* uuid);

// Provided by implementation

#ifdef __cplusplus
}
#endif
