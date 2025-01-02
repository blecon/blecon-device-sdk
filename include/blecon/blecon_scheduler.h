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
#include "blecon_task_queue.h"
#include "port/blecon_timer.h"

struct blecon_timeout_t;

// A timeout scheduler can only be accessed safely from the scheduler's event loop thread

struct blecon_scheduler_t {
    struct blecon_task_queue_t task_queue;
    struct blecon_timer_t* timer;
    struct blecon_event_t* event;

    // Linked list
    struct blecon_list_t timeouts;
};

struct blecon_timeout_t {
    // Underlying task
    struct blecon_task_t task;

    struct blecon_scheduler_t* scheduler;
    uint64_t deadline;
};

void blecon_scheduler_init(struct blecon_scheduler_t* scheduler, struct blecon_event_loop_t* event_loop, struct blecon_timer_t* timer);

void blecon_scheduler_cleanup(struct blecon_scheduler_t* scheduler);

void blecon_scheduler_queue_timeout(struct blecon_scheduler_t* scheduler, struct blecon_timeout_t* timeout, uint32_t timeout_ms);

void blecon_timeout_init(struct blecon_timeout_t* timeout, blecon_task_callback_t callback, void* callback_user_data);

void blecon_timeout_cancel(struct blecon_timeout_t* timeout);

// Convenience API wrapping the underlying scheduler API
static inline void blecon_scheduler_queue_task(struct blecon_scheduler_t* scheduler, struct blecon_task_t* task) {
    blecon_task_queue_push(&scheduler->task_queue, task);
}

static inline struct blecon_task_queue_t* blecon_scheduler_get_task_queue(struct blecon_scheduler_t* scheduler) {
    return &scheduler->task_queue;
}

static inline uint64_t blecon_scheduler_get_monotonic_time(struct blecon_scheduler_t* scheduler) {
    return blecon_timer_get_monotonic_time(scheduler->timer);
}


#ifdef __cplusplus
}
#endif
