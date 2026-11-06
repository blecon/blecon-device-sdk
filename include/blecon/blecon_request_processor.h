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

#include "blecon_list.h"
#include "blecon_request.h"
#include "blecon_task_queue.h"

struct blecon_request_processor_t;
struct blecon_request_t;
struct blecon_request_frame_t;
struct blecon_tracing_channel_t;

struct blecon_request_processor_client_t;
struct blecon_request_processor_client_callbacks_t {
    void (*on_connection)(struct blecon_request_processor_client_t* client);
    void (*on_disconnection)(struct blecon_request_processor_client_t* client);
};

struct blecon_request_processor_client_t {
    struct blecon_request_processor_t* processor;
    const struct blecon_request_processor_client_callbacks_t* callbacks;
    void* user_data;
    bool connection_target;
    struct blecon_list_node_t node;
};

struct blecon_request_processor_fn_t {
    // Ask the owner to acquire (or release) a connection; false = refused, which the processor
    // undoes by rolling back the vote that triggered the call. Must not synchronously call back
    // into the client vote APIs -- the rollback runs after this returns, so a re-entrant vote
    // would be counted twice.
    bool (*set_connection_target)(struct blecon_request_processor_t* processor, bool connection_target);
    bool (*can_send_frame)(struct blecon_request_processor_t* processor);
    bool (*frame_send)(struct blecon_request_processor_t* processor, struct blecon_request_frame_t* frame);
};

struct blecon_request_processor_t {
    const struct blecon_request_processor_fn_t* fn;
    struct blecon_task_queue_t* task_queue;
    struct blecon_task_t process_task;
    bool process_task_scheduled;
    bool connected;
    size_t connection_target_count;

    // Whether the owner is believed to be holding a connection target. Separate from
    // connection_target_count because the two can disagree: a refused re-dial leaves clients
    // voting for a connection the owner is not holding, a refused release leaves it holding one
    // nobody votes for. An initiate asks the owner whenever it is not holding one, which turns
    // a refused re-dial into a retry rather than a permanent loss.
    bool owner_connection_target;
    struct blecon_list_t requests_list;
    blecon_request_id_t next_request_id;
    struct blecon_list_t clients_list;

    struct blecon_tracing_channel_t* tracing_channel;

    // Live client.connection span id, relayed in by whichever composition root owns both
    // this processor and the connection. Each request.request span links to it at submit
    // time; 0 means no link.
    blecon_tracing_id_t client_connection_span_id;
};

enum blecon_request_processor_event_t {
    blecon_request_processor_event_none,
    blecon_request_processor_event_connected,
    blecon_request_processor_event_disconnected,
};

// tracing_channel: the channel request.request spans are emitted to; NULL for a processor
// whose owner has none (its requests then open no span).
void blecon_request_processor_init(struct blecon_request_processor_t* processor, const struct blecon_request_processor_fn_t* fn, struct blecon_task_queue_t* task_queue, struct blecon_tracing_channel_t* tracing_channel);

// Relay the live client.connection span id (0 to clear) that request.request spans
// submitted through this processor link to.
void blecon_request_processor_set_client_connection_span_id(struct blecon_request_processor_t* processor, blecon_tracing_id_t span_id);

void blecon_request_processor_add_client(struct blecon_request_processor_t* processor, struct blecon_request_processor_client_t* client, const struct blecon_request_processor_client_callbacks_t* callbacks, void* user_data);

// Remove a client, withdrawing any vote it still holds -- even if the owner refuses the
// terminate it issues, leaving it holding a connection nobody votes for until the next
// vote takes the count 0->1->0.
void blecon_request_processor_remove_client(struct blecon_request_processor_t* processor, struct blecon_request_processor_client_t* client);

// Cast/withdraw this client's vote for a connection. Both return false when the owner
// refused the transition, leaving the client's vote unchanged so a later call retries it.
// A client that already votes may call initiate again to re-drive a connection the owner
// refused to acquire; once the owner holds one it returns true without asking again.
bool blecon_request_processor_client_connection_initiate(struct blecon_request_processor_client_t* client);

bool blecon_request_processor_client_connection_terminate(struct blecon_request_processor_client_t* client);

bool blecon_request_processor_client_is_connected(struct blecon_request_processor_client_t* client);

void blecon_request_processor_submit(struct blecon_request_processor_t* processor, struct blecon_request_t* request);

void blecon_request_processor_process(struct blecon_request_processor_t* processor, enum blecon_request_processor_event_t event);

void blecon_request_processor_receive_frame(struct blecon_request_processor_t* processor, struct blecon_request_frame_t* frame);

uint8_t* blecon_request_processor_data_buffer_alloc(struct blecon_request_processor_t* processor, size_t sz, uint16_t request_id);

struct blecon_request_processor_t* blecon_request_processor_client_get_processor(struct blecon_request_processor_client_t* client);

void* blecon_request_processor_client_get_user_data(struct blecon_request_processor_client_t* client);

#ifdef __cplusplus
}
#endif
