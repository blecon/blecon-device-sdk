/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include "blecon_list.h"
#include "port/blecon_event_loop.h"
#include "port/blecon_mutex.h"

struct blecon_task_t;

struct blecon_task_queue_t {
    struct blecon_event_t* event;

    bool is_processing;

    // Linked list
    struct blecon_list_t tasks;
    struct blecon_mutex_t* mutex;
};

typedef void (*blecon_task_callback_t)(struct blecon_task_t* task, void* user_data);

struct blecon_task_t {
    blecon_task_callback_t callback;
    void* callback_user_data;

    struct blecon_task_queue_t* task_queue;

    // Linked list
    struct blecon_list_node_t node;
};

void blecon_task_queue_init(struct blecon_task_queue_t* task_queue, struct blecon_event_loop_t* event_loop);
void blecon_task_queue_cleanup(struct blecon_task_queue_t* task_queue);

void blecon_task_queue_push(struct blecon_task_queue_t* task_queue, struct blecon_task_t* task);

static inline struct blecon_event_loop_t* blecon_task_queue_get_event_loop(struct blecon_task_queue_t* task_queue) {
    return task_queue->event->event_loop;
}

void blecon_task_init(struct blecon_task_t* task, blecon_task_callback_t callback, void* callback_user_data);
void blecon_task_cancel(struct blecon_task_t* task);

#ifdef __cplusplus
}
#endif
