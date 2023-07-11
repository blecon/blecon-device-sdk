/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "stddef.h"
#include "stdbool.h"
#include "blecon_buffer.h"

struct blecon_buffer_node_header_t {
    struct blecon_buffer_t next;
};

struct blecon_buffer_queue_t {
    struct blecon_buffer_node_header_t head;
    size_t queue_sz;
};

void blecon_buffer_queue_init(struct blecon_buffer_queue_t* buffer_queue);

struct blecon_buffer_t blecon_buffer_queue_alloc(size_t sz);

void blecon_buffer_queue_push(struct blecon_buffer_queue_t* buffer_queue, struct blecon_buffer_t buf);

struct blecon_buffer_t blecon_buffer_queue_pop(struct blecon_buffer_queue_t* buffer_queue);

struct blecon_buffer_t blecon_buffer_queue_peek(struct blecon_buffer_queue_t* buffer_queue);

bool blecon_buffer_queue_is_empty(struct blecon_buffer_queue_t* buffer_queue);

size_t blecon_buffer_queue_size(struct blecon_buffer_queue_t* buffer_queue);
