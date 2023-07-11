/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stdlib.h"
#include "string.h"
#include "assert.h"

#include "blecon_zephyr_event_loop.h"

#include "blecon/blecon_memory.h"
#include "blecon/blecon_error.h"

#include "zephyr/kernel.h"

#define BLECON_EVENT_SIGNAL     (1u << 0u)
#define BLECON_EVENT_TIMEOUT    (1u << 1u)
#define BLECON_EVENT_BREAK      (1u << 2u)

#define BLECON_ZEPHYR_EVENT_LOOP_MAX_USER_EVENTS        16u
#define BLECON_ZEPHYR_EVENT_LOOP_FIRST_USER_EVENT_ID    3u

struct blecon_zephyr_event_loop_t;

static void blecon_zephyr_event_loop_setup(struct blecon_event_loop_t* event_loop);
static void blecon_zephyr_event_loop_run(struct blecon_event_loop_t* event_loop);
static uint32_t blecon_zephyr_event_loop_get_ticks(struct blecon_event_loop_t* event_loop);
static uint32_t blecon_zephyr_event_loop_ms_to_ticks(struct blecon_event_loop_t* event_loop, uint32_t ms);
static void blecon_zephyr_event_loop_set_timeout(struct blecon_event_loop_t* event_loop, uint32_t ticks);
static void blecon_zephyr_event_loop_cancel_timeout(struct blecon_event_loop_t* event_loop);
static void blecon_zephyr_event_loop_signal(struct blecon_event_loop_t* event_loop);
static void blecon_zephyr_event_loop_lock(struct blecon_event_loop_t* event_loop);
static void blecon_zephyr_event_loop_unlock(struct blecon_event_loop_t* event_loop);

static void blecon_zephyr_event_loop_on_timeout(struct k_timer* timer);

struct blecon_zephyr_user_event_t {
    blecon_zephyr_event_callback_t callback;
    void* user_data;
};

struct blecon_zephyr_event_loop_t {
    struct blecon_event_loop_t event_loop;
    struct k_mutex z_mutex;
    struct k_event z_event;
    struct k_timer z_timer;
    size_t assigned_user_events;
    struct blecon_zephyr_user_event_t user_events[BLECON_ZEPHYR_EVENT_LOOP_MAX_USER_EVENTS];
};

struct blecon_event_loop_t* blecon_zephyr_event_loop_new(void) {
    static const struct blecon_event_loop_fn_t event_loop_fn = {
        .setup = blecon_zephyr_event_loop_setup,
        .run = blecon_zephyr_event_loop_run,
        .get_ticks = blecon_zephyr_event_loop_get_ticks,
        .ms_to_ticks = blecon_zephyr_event_loop_ms_to_ticks,
        .set_timeout = blecon_zephyr_event_loop_set_timeout,
        .cancel_timeout = blecon_zephyr_event_loop_cancel_timeout,
        .signal = blecon_zephyr_event_loop_signal,
        .lock = blecon_zephyr_event_loop_lock,
        .unlock = blecon_zephyr_event_loop_unlock,
    };

    struct blecon_zephyr_event_loop_t* zephyr_event_loop = BLECON_ALLOC(sizeof(struct blecon_zephyr_event_loop_t));
    if(zephyr_event_loop == NULL) {
        blecon_fatal_error();
    }

    blecon_event_loop_init(&zephyr_event_loop->event_loop, &event_loop_fn);

    k_mutex_init(&zephyr_event_loop->z_mutex);
    k_event_init(&zephyr_event_loop->z_event);
    k_timer_init(&zephyr_event_loop->z_timer, blecon_zephyr_event_loop_on_timeout, NULL);

    zephyr_event_loop->assigned_user_events = 0;

    return &zephyr_event_loop->event_loop;
}

uint32_t blecon_zephyr_event_loop_assign_event(struct blecon_event_loop_t* event_loop, blecon_zephyr_event_callback_t callback, void* user_data) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;
    blecon_assert(zephyr_event_loop->assigned_user_events < BLECON_ZEPHYR_EVENT_LOOP_MAX_USER_EVENTS);
    
    // Allocate a free event id
    uint32_t event_id = zephyr_event_loop->assigned_user_events;
    zephyr_event_loop->assigned_user_events++;
    zephyr_event_loop->user_events[event_id].callback = callback;
    zephyr_event_loop->user_events[event_id].user_data = user_data;

    return event_id;
}

void blecon_zephyr_event_loop_post_event(struct blecon_event_loop_t* event_loop, uint32_t event_id) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;
    blecon_assert(event_id <= zephyr_event_loop->assigned_user_events);

    k_event_post(&zephyr_event_loop->z_event, 1u << (BLECON_ZEPHYR_EVENT_LOOP_FIRST_USER_EVENT_ID + event_id));
}

void blecon_zephyr_event_loop_break(struct blecon_event_loop_t* event_loop) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;

    k_event_post(&zephyr_event_loop->z_event, BLECON_EVENT_BREAK);
}

void blecon_zephyr_event_loop_setup(struct blecon_event_loop_t* event_loop) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;
    (void)zephyr_event_loop;
}

void blecon_zephyr_event_loop_run(struct blecon_event_loop_t* event_loop) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;

    while(true) {
        uint32_t event_mask = ~(UINT32_MAX << (BLECON_ZEPHYR_EVENT_LOOP_FIRST_USER_EVENT_ID + zephyr_event_loop->assigned_user_events));

        // Wait for events
        uint32_t events = k_event_wait(&zephyr_event_loop->z_event, event_mask, false, K_FOREVER);

        // Clear raised events (not done automatically by k_event_wait())
        k_event_set_masked(&zephyr_event_loop->z_event, 0, events);

        // Call handler if signal or timeout events were raised
        if( (events & (BLECON_EVENT_SIGNAL | BLECON_EVENT_TIMEOUT)) != 0 ) {
            k_mutex_lock(&zephyr_event_loop->z_mutex, K_FOREVER);
            blecon_event_loop_on_event(event_loop, (events & BLECON_EVENT_TIMEOUT) != 0);
            k_mutex_unlock(&zephyr_event_loop->z_mutex);
        }

        // Call any user event callback
        uint32_t user_events = events >> BLECON_ZEPHYR_EVENT_LOOP_FIRST_USER_EVENT_ID;
        for(size_t p = 0; p < zephyr_event_loop->assigned_user_events; p++) {
            if( user_events & 1u ) {
                zephyr_event_loop->user_events[p].callback(&zephyr_event_loop->event_loop, zephyr_event_loop->user_events[p].user_data);
            }
            user_events >>= 1u;
        }

        // If the break event was raised, return from loop
        if( events & BLECON_EVENT_BREAK ) {
            return;
        }
    }
}

uint32_t blecon_zephyr_event_loop_get_ticks(struct blecon_event_loop_t* event_loop) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;
    (void)zephyr_event_loop;

    int64_t now = k_uptime_get();

    uint32_t ticks = (uint32_t)((uint64_t)now & (uint64_t)(~(uint32_t)0));
    return ticks;
}

uint32_t blecon_zephyr_event_loop_ms_to_ticks(struct blecon_event_loop_t* event_loop, uint32_t ms) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;
    (void)zephyr_event_loop;

    return ms;
}

void blecon_zephyr_event_loop_set_timeout(struct blecon_event_loop_t* event_loop, uint32_t ticks) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;

    k_timer_start(&zephyr_event_loop->z_timer, K_MSEC(ticks), K_FOREVER);
}

void blecon_zephyr_event_loop_cancel_timeout(struct blecon_event_loop_t* event_loop) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;

    k_timer_stop(&zephyr_event_loop->z_timer);
}

void blecon_zephyr_event_loop_signal(struct blecon_event_loop_t* event_loop) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;

    k_event_post(&zephyr_event_loop->z_event, BLECON_EVENT_SIGNAL);
}

void blecon_zephyr_event_loop_lock(struct blecon_event_loop_t* event_loop) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;
    k_mutex_lock(&zephyr_event_loop->z_mutex, K_FOREVER);
}

void blecon_zephyr_event_loop_unlock(struct blecon_event_loop_t* event_loop) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;
    k_mutex_unlock(&zephyr_event_loop->z_mutex);
}

void blecon_zephyr_event_loop_on_timeout(struct k_timer* timer) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop 
        = (struct blecon_zephyr_event_loop_t*) ((char*)timer - offsetof(struct blecon_zephyr_event_loop_t, z_timer));

    k_event_post(&zephyr_event_loop->z_event, BLECON_EVENT_TIMEOUT);
}

