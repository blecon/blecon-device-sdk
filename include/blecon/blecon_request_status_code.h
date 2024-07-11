/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#define BLECON_REQUEST_LOCAL_STATUS_CODE_NS 0x100
#define BLECON_REQUEST_REMOTE_STATUS_CODE_NS 0x200
#define BLECON_REQUEST_STATUS_CODE_NS_MASK 0xFF00

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

#ifdef __cplusplus
}
#endif
