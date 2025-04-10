/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
/** @file blecon.h
    @brief The Blecon SDK API.
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
#include "blecon_modem.h"
#include "blecon_request.h"
#include "blecon_request_processor.h"
#include "blecon_task_queue.h"

struct blecon_event_loop_t;
struct blecon_timer_t;
struct blecon_bluetooth_t;
struct blecon_crypto_t;
struct blecon_nvm_t;
struct blecon_nfc_t;
struct blecon_ext_modem_transport_t;

struct blecon_t;
struct blecon_modem_t;
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

    /**
     * @brief Called when time is updated from the Blecon infrastructure.
     * @param blecon the blecon instance
     */
    void (*on_time_update)(struct blecon_t* blecon);

    /**
     * @brief Called when a ping is completed.
     * @param blecon the blecon instance
     */
    void (*on_ping_result)(struct blecon_t* blecon);

    /**
     * @brief Called when a scan report is available.
     * @param blecon the blecon instance
     */
    void (*on_scan_report)(struct blecon_t* blecon);

    /**
     * @brief Called when a scan completed.
     * @param blecon the blecon instance
     */
    void (*on_scan_complete)(struct blecon_t* blecon);
};

/** 
 * @brief Create new internal modem instance
 * 
 * @param event_loop a platform-specific event loop instance
 * @param timer a platform-specific timer instance
 * @param bluetooth a platform-specific bluetooth instance
 * @param crypto a platform-specific crypto instance
 * @param nvm a platform-specific nvm instance
 * @param nfc a platform-specific nfc instance
 * @param scan_buffer_size the size of the scan buffer
 * @param allocator a function to allocate memory
 * @return a pointer to a struct blecon_modem_t instance
 * */
struct blecon_modem_t* blecon_int_modem_create(
    struct blecon_event_loop_t* event_loop,
    struct blecon_timer_t* timer,
    struct blecon_bluetooth_t* bluetooth,
    struct blecon_crypto_t* crypto,
    struct blecon_nvm_t* nvm,
    struct blecon_nfc_t* nfc,
    size_t scan_buffer_size,
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
 * @brief Get the current global synchronized time
 *
 * @param blecon the blecon instance
 * @param time_valid a pointer to a bool which is set to true if the time is valid
 * @param utc_time_ms_now a pointer to a uint64_t where the current time is stored
 * @param utc_time_ms_last_updated a pointer to a uint64_t where the time of last update is stored
 * @return true on success, or false on failure
 */
bool blecon_get_time(struct blecon_t* blecon, bool* time_valid, uint64_t* utc_time_ms_now, uint64_t* utc_time_ms_last_updated);

/**
 * @brief Perform a ping to the Blecon infrastructure
 * 
 * @param blecon the blecon instance
 * @param timeout_ms the timeout in milliseconds
 * @return true on success, or false on failure
 */
bool blecon_ping_perform(struct blecon_t* blecon, uint32_t timeout_ms);

/**
 * @brief Cancel a ping to the Blecon infrastructure
 * 
 * @param blecon the blecon instance
 * @return true on success, or false on failure
 */
bool blecon_ping_cancel(struct blecon_t* blecon);

/**
 * @brief Get latency information from the last ping
 * 
 * @param blecon the blecon instance
 * @param latency_available a pointer to a bool which is set to true if the latency is available
 * @param connection_latency_ms a pointer to a uint32_t where the connection latency is stored (milliseconds)
 * @param round_trip_latency_ms a pointer to a uint32_t where the round trip latency is stored (milliseconds)
 * @return true on success, or false on failure
 */
bool blecon_ping_get_latency(struct blecon_t* blecon, bool* latency_available, uint32_t* connection_latency_ms, uint32_t* round_trip_latency_ms);

/**
 * @brief Start a peer scanning session
 * 
 * @param blecon the blecon instance
 * @param peer_scan true to report peer devices
 * @param raw_scan true to report raw advertising data
 * @param duration_ms the duration of the scan in milliseconds
 * @return true on success, or false on failure
 */
bool blecon_scan_start(struct blecon_t* blecon, bool peer_scan, bool raw_scan, uint32_t duration_ms);

/**
 * @brief Stop the current peer scanning session
 * 
 * @param blecon the blecon instance
 * @return true on success, or false on failure
 */
bool blecon_scan_stop(struct blecon_t* blecon);

/**
 * @brief Get the data from the last peer scan
 * 
 * @param blecon the blecon instance
 * 
 * @param peer_scan_report_iterator a function to call for each peer device report; if NULL, no peer device reports are returned
 * @param raw_scan_report_iterator a function to call for each raw advertising data report; if NULL, no raw advertising data reports are returned
 * @param overflow a pointer to a bool which is set to true if the scan data buffer overflowed
 * @param user_data user data to pass to the report iterator callbacks
 * @return true on success, or false on failure
 */
bool blecon_scan_get_data(struct blecon_t* blecon, 
    void (*peer_scan_report_iterator)(const struct blecon_modem_peer_scan_report_t* report, void* user_data), 
    void (*raw_scan_report_iterator)(const struct blecon_modem_raw_scan_report_t* report, void* user_data),
    bool* overflow, void* user_data);

/**
 * @brief Submit a request for processing
 * 
 * @param blecon the blecon instance
 * @param request the request to submit
 */
void blecon_submit_request(struct blecon_t* blecon, struct blecon_request_t* request);

/**
 * @brief Get a request processor
 * 
 * @param blecon the blecon instance
 * @return a pointer to a struct blecon_request_processor_t instance
 */
struct blecon_request_processor_t* blecon_get_request_processor(struct blecon_t* blecon);

struct blecon_t {
    struct blecon_modem_t* modem;
    const struct blecon_callbacks_t* callbacks;
    void* user_data;

    bool is_setup;
    bool is_connected;
    struct blecon_modem_info_t modem_info;
    struct blecon_task_queue_t task_queue;
    struct blecon_request_processor_t request_processor;
    struct blecon_request_processor_client_t request_processor_client;
    size_t outgoing_request_frame_queue_space_count;
    void (*peer_scan_report_iterator)(const struct blecon_modem_peer_scan_report_t*, void*);
    void (*raw_scan_report_iterator)(const struct blecon_modem_raw_scan_report_t*, void*);
    void* iterators_user_data;
};

#ifdef __cplusplus
}
#endif

/**
 * @}
 */
