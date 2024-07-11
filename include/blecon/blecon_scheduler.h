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

struct blecon_scheduler_t {
    struct blecon_event_loop_t* event_loop;

    bool is_processing;
    // uint32_t last_processed_ticks;

    bool is_timeout_set;
    uint64_t timeout_deadline;

    // Linked list
    struct blecon_list_t tasks;
    struct blecon_mutex_t* mutex;
};

typedef void (*blecon_task_callback_t)(struct blecon_task_t* task, void* user_data);

struct blecon_task_t {
    uint64_t deadline;

    blecon_task_callback_t callback;
    void* callback_user_data;

    struct blecon_scheduler_t* scheduler;

    // Linked list
    struct blecon_list_node_t node;
};

void blecon_scheduler_init(struct blecon_scheduler_t* scheduler, struct blecon_event_loop_t* event_loop);
void blecon_scheduler_cleanup(struct blecon_scheduler_t* scheduler);

void blecon_scheduler_queue_task(struct blecon_task_t* task, struct blecon_scheduler_t* scheduler, blecon_task_callback_t callback, void* callback_user_data);
void blecon_scheduler_queue_delayed_task(struct blecon_task_t* task, struct blecon_scheduler_t* scheduler, blecon_task_callback_t callback, void* callback_user_data, uint32_t timeout_ms);
void blecon_scheduler_cancel_task(struct blecon_task_t* task);
void blecon_scheduler_run(struct blecon_scheduler_t* scheduler);
void blecon_scheduler_task_init(struct blecon_task_t* task);

#ifdef __cplusplus
}
#endif
