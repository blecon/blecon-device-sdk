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

#define BLECON_MEMFAULT_CLIENT_CONCURRENT_SEND_DATA_OPS_COUNT 2

struct blecon_memfault_client_send_data_op_t {
    struct blecon_request_send_data_op_t op;
    uint8_t* buffer;
    bool busy;
    struct blecon_memfault_client_t* client;
};

struct blecon_memfault_client_t {
    struct blecon_request_parameters_t request_params;
    struct blecon_request_processor_client_t request_processor_client;
    struct blecon_memfault_t* memfault;
    struct blecon_request_t request;
    struct blecon_memfault_client_send_data_op_t send_ops[BLECON_MEMFAULT_CLIENT_CONCURRENT_SEND_DATA_OPS_COUNT];
    bool send_finished;
    struct blecon_request_receive_data_op_t receive_op;
    uint8_t receive_buffer[16];
};

// Register the client on the request processor. At each connection, whoever made it, the client
// uploads the data Memfault holds through the port, one chunk per request: a chunk is what the
// port's get_next_chunk() begins and get_chunk_data() reads out, in packets of up to BLECON_MTU
// bytes. The Zephyr port runs Memfault's packetizer in its multi-packet mode, so there a chunk is
// one whole Memfault message (a coredump, events, logs), in as many packets as it needs. The client
// casts a connection vote once it has found a chunk to send, and withdraws it the next time it
// looks and finds none, after a request completes or at the next connection.
//
// A failed request is not retried. A lost link fails the request in flight; the vote stays, so the
// device dials again at once and the next connection resumes the upload, but the chunk that request
// carried is not sent again. If the client had fetched its last packet, Memfault's packetizer
// marked it read at that fetch. If not (only a chunk longer than the
// BLECON_MEMFAULT_CLIENT_CONCURRENT_SEND_DATA_OPS_COUNT packets the client fetches ahead of the
// modem can be cut that early, in practice a coredump), the next connection's first request carries
// the rest of it without its start, which the Memfault service drops. The whole messages queued
// behind it then follow as usual. A request the network answers with an error status loses its
// chunk too (nothing resends it) and keeps the vote, but the client sends nothing more on that
// connection; the next one comes once the hotspot drops the idle link (5 s without traffic), so a
// persistent error discards the backlog one chunk per connection.
//
// A Blecon hotspot ends every device connection 30 s after it is up, so a message whose upload
// takes longer than that, such as a large coredump on a slow link, never arrives.
void blecon_memfault_client_init(struct blecon_memfault_client_t* client, struct blecon_request_processor_t* request_processor, struct blecon_memfault_t* memfault, const uint8_t* blecon_id, const char* request_namespace);

// Release what the client holds: the request in flight, detached from the processor with no
// callback raised, the buffers of its send operations, and the client's registration on the
// request processor, which withdraws the connection vote it holds.
void blecon_memfault_client_cleanup(struct blecon_memfault_client_t* client);

#ifdef __cplusplus
}
#endif
