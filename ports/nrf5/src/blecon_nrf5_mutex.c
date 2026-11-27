/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stdlib.h"
#include "string.h"

#include "blecon/blecon_error.h"
#include "blecon_nrf5/blecon_nrf5_mutex.h"

#include "app_util_platform.h"

// The nRF5 port runs one thread of execution (no RTOS), so a mutex between threads has
// nothing to exclude and these are no-ops. The thread safety the task queue promises
// (blecon_task_queue_push, blecon_scheduler_queue_task) is not interrupt safety, here as
// on every port (Zephyr's k_mutex may not be taken in an ISR): an interrupt handler defers to
// app_scheduler first, as the nRF5 apps' button handlers do, or signals an event
// (blecon_event_signal, which the event loop takes from interrupt context). A lock
// taken in handler mode would race the loop's own pass over the task list, so it is fatal.

struct blecon_mutex_t* blecon_mutex_new(void) {
    return NULL;
}

void blecon_mutex_lock(struct blecon_mutex_t* mutex) {
    (void) mutex;
    blecon_assert(current_int_priority_get() == APP_IRQ_PRIORITY_THREAD);
}

void blecon_mutex_unlock(struct blecon_mutex_t* mutex) {
    (void) mutex;
}

void blecon_mutex_free(struct blecon_mutex_t* mutex) {
    (void) mutex;
}
