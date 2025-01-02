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

struct blecon_nrf5_event_loop_t {
    struct blecon_event_loop_t event_loop;
    struct blecon_event_t events[BLECON_NRF5_EVENT_LOOP_MAX_EVENTS];
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
    _event_loop.event_count++;

    return event;
}

void blecon_nrf5_event_loop_run(struct blecon_event_loop_t* event_loop) {
    while(true) {
        app_sched_execute();

        // USB event queue needs to be processed here if enabled
        #if NRF_MODULE_ENABLED(APP_USBD)
        app_usbd_event_queue_process();
        #endif

        if( NRF_LOG_PROCESS() == false )
        {
            nrf_pwr_mgmt_run();
        }
    }
}

void blecon_nrf5_event_loop_lock(struct blecon_event_loop_t* event_loop) {
    // No need to lock
}

void blecon_nrf5_event_loop_unlock(struct blecon_event_loop_t* event_loop) {
    // No need to unlock
}

void blecon_nrf5_event_loop_signal(struct blecon_event_loop_t* event_loop, struct blecon_event_t* event) {
    app_sched_event_put(&event, sizeof(struct blecon_event_t*), blecon_nrf5_signal_handler);
}

void blecon_nrf5_signal_handler(void* p_event_data, uint16_t event_size) {
    blecon_assert(event_size == sizeof(struct blecon_event_t*));
    struct blecon_event_t* event = NULL;
    memcpy(&event, p_event_data, sizeof(struct blecon_event_t*));

    blecon_event_on_raised(event);
}