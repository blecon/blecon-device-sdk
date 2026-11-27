/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blecon_nrf5_event_loop.h"
#include "blecon/blecon_scheduler.h"
#include "blecon/blecon_error.h"

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_sdm.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_pwr_mgmt.h"
#include "app_timer.h"
#include "app_scheduler.h"
#include "app_util_platform.h"

#define BLECON_NRF5_EVENT_LOOP_MAX_EVENTS 16

// Validate nRF5 SDK Config
#if !APP_SCHEDULER_ENABLED
#error "APP_SCHEDULER_ENABLED must be enabled in sdk_config.h"
#endif

#if NRF_MODULE_ENABLED(APP_USBD)
#include "app_usbd.h"
#endif

static void blecon_nrf5_event_loop_setup(struct blecon_event_loop_t* event_loop);
static struct blecon_event_t* blecon_nrf5_event_loop_register_event(struct blecon_event_loop_t* event_loop);
static void blecon_nrf5_event_loop_run(struct blecon_event_loop_t* event_loop);
static void blecon_nrf5_event_loop_lock(struct blecon_event_loop_t* event_loop);
static void blecon_nrf5_event_loop_unlock(struct blecon_event_loop_t* event_loop);
static void blecon_nrf5_event_loop_signal(struct blecon_event_loop_t* event_loop, struct blecon_event_t* event);

static void blecon_nrf5_signal_handler(void* p_event_data, uint16_t event_size);
static size_t blecon_nrf5_event_loop_event_index(const struct blecon_event_t* event);

struct blecon_nrf5_event_loop_t {
    struct blecon_event_loop_t event_loop;
    struct blecon_event_t events[BLECON_NRF5_EVENT_LOOP_MAX_EVENTS];
    // Set while an event's signal sits in the scheduler queue (written from interrupt context)
    volatile bool events_pending[BLECON_NRF5_EVENT_LOOP_MAX_EVENTS];
    size_t event_count;
};

static struct blecon_nrf5_event_loop_t _event_loop;

struct blecon_event_loop_t* blecon_nrf5_event_loop_init(void) {
    static const struct blecon_event_loop_fn_t event_loop_fn = {
        .setup = blecon_nrf5_event_loop_setup,
        .register_event = blecon_nrf5_event_loop_register_event,
        .run = blecon_nrf5_event_loop_run,
        .lock = blecon_nrf5_event_loop_lock,
        .unlock = blecon_nrf5_event_loop_unlock,
        .signal = blecon_nrf5_event_loop_signal,
    };

    blecon_event_loop_init(&_event_loop.event_loop, &event_loop_fn);

    _event_loop.event_count = 0;

    return &_event_loop.event_loop;
}

void blecon_nrf5_event_loop_setup(struct blecon_event_loop_t* event_loop) {
}

struct blecon_event_t* blecon_nrf5_event_loop_register_event(struct blecon_event_loop_t* event_loop) {
    blecon_assert(_event_loop.event_count < BLECON_NRF5_EVENT_LOOP_MAX_EVENTS);

    struct blecon_event_t* event = &_event_loop.events[_event_loop.event_count];
    _event_loop.events_pending[_event_loop.event_count] = false;
    _event_loop.event_count++;

    return event;
}

void blecon_nrf5_event_loop_run(struct blecon_event_loop_t* event_loop) {
    while( true ) {
        app_sched_execute();

// USB event queue needs to be processed here if enabled
#if NRF_MODULE_ENABLED(APP_USBD)
        app_usbd_event_queue_process();
#endif

        if( NRF_LOG_PROCESS() == false ) {
            nrf_pwr_mgmt_run();
        }
    }
}

void blecon_nrf5_event_loop_lock(struct blecon_event_loop_t* event_loop) {
    // One thread, so nothing to exclude; an interrupt handler may not take it, as on Zephyr, where
    // it is a k_mutex (see blecon_nrf5_mutex.c), and doing so is fatal rather than a silent race
    blecon_assert(current_int_priority_get() == APP_IRQ_PRIORITY_THREAD);
}

void blecon_nrf5_event_loop_unlock(struct blecon_event_loop_t* event_loop) {
    // No need to unlock
}

void blecon_nrf5_event_loop_signal(struct blecon_event_loop_t* event_loop, struct blecon_event_t* event) {
    // A signal coalesces with one already queued for the same event, as libev's, Zephyr's
    // and the host's external loop do, so the loop holds at most one scheduler entry per
    // event plus the one whose handler is running (the scheduler releases a slot after its
    // handler returns). A full queue then means the application sized it below that: fatal
    // rather than a signal lost silently. Signals come from interrupt context too
    // (app_timer's RTC handler), so the test-and-set and the put share one critical region.
    size_t index = blecon_nrf5_event_loop_event_index(event);
    ret_code_t err_code = NRF_SUCCESS;

    CRITICAL_REGION_ENTER();
    if( !_event_loop.events_pending[index] ) {
        _event_loop.events_pending[index] = true;
        err_code = app_sched_event_put(&event, sizeof(struct blecon_event_t*), blecon_nrf5_signal_handler);
    }
    CRITICAL_REGION_EXIT();

    blecon_assert(err_code == NRF_SUCCESS);
}

void blecon_nrf5_signal_handler(void* p_event_data, uint16_t event_size) {
    blecon_assert(event_size == sizeof(struct blecon_event_t*));
    struct blecon_event_t* event = NULL;
    memcpy(&event, p_event_data, sizeof(struct blecon_event_t*));

    // Cleared before the callback runs, so a signal raised from it (or from an interrupt
    // meanwhile) queues the event again
    _event_loop.events_pending[blecon_nrf5_event_loop_event_index(event)] = false;

    blecon_event_on_raised(event);
}

size_t blecon_nrf5_event_loop_event_index(const struct blecon_event_t* event) {
    // Compared as integers: a pointer comparison is only defined within one array, and a foreign pointer is what the assert catches
    uintptr_t address = (uintptr_t) event;
    uintptr_t first = (uintptr_t) &_event_loop.events[0];
    uintptr_t end = (uintptr_t) &_event_loop.events[_event_loop.event_count];
    blecon_assert((address >= first) && (address < end) && (((address - first) % sizeof(struct blecon_event_t)) == 0));
    return (size_t) ((address - first) / sizeof(struct blecon_event_t));
}
