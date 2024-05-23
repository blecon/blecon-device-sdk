/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define BLECON_RPC_STATUS_CODE_NS_MASK 0xff00
#define BLECON_RPC_LOCAL_STATUS_CODE_NS 0x100
#define BLECON_RPC_REMOTE_STATUS_CODE_NS 0x200

enum blecon_rpc_status_code_t {
    blecon_rpc_ok = 0,
    
    // Local status codes
    blecon_rpc_status_pending = BLECON_RPC_LOCAL_STATUS_CODE_NS + 0,
    blecon_rpc_error_timeout = BLECON_RPC_LOCAL_STATUS_CODE_NS + 1,
    blecon_rpc_error_transport_error = BLECON_RPC_LOCAL_STATUS_CODE_NS + 2,
    
    // Remote status codes
    blecon_rpc_error_handler_not_set = BLECON_RPC_REMOTE_STATUS_CODE_NS + 0,
    blecon_rpc_error_handler_timeout = BLECON_RPC_REMOTE_STATUS_CODE_NS + 1,
    blecon_rpc_error_handler_failed = BLECON_RPC_REMOTE_STATUS_CODE_NS + 2,
    blecon_rpc_error_network_not_set = BLECON_RPC_REMOTE_STATUS_CODE_NS + 3
};

#ifdef __cplusplus
}
#endif
