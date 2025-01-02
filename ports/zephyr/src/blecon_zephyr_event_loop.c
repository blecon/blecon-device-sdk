/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stdlib.h"
#include "string.h"

#include "blecon_zephyr_event_loop.h"

#include "blecon/blecon_memory.h"
#include "blecon/blecon_error.h"

#include "zephyr/kernel.h"

#define BLECON_EVENT_BREAK      (1u << 31u)
#define BLECON_ZEPHYR_EVENT_LOOP_MAX_EVENTS        16u

struct blecon_zephyr_event_loop_t;

static void blecon_zephyr_event_loop_setup(struct blecon_event_loop_t* event_loop);
static struct blecon_event_t* blecon_zephyr_event_loop_register_event(struct blecon_event_loop_t* event_loop);
static void blecon_zephyr_event_loop_run(struct blecon_event_loop_t* event_loop);
static void blecon_zephyr_event_loop_lock(struct blecon_event_loop_t* event_loop);
static void blecon_zephyr_event_loop_unlock(struct blecon_event_loop_t* event_loop);
static void blecon_zephyr_event_loop_signal(struct blecon_event_loop_t* event_loop, struct blecon_event_t* event);

struct blecon_zephyr_event_t {
    struct blecon_event_t event;
};

struct blecon_zephyr_event_loop_t {
    struct blecon_event_loop_t event_loop;
    struct k_mutex z_mutex;
    struct k_event z_event;
    struct blecon_zephyr_event_t events[BLECON_ZEPHYR_EVENT_LOOP_MAX_EVENTS];
    size_t events_count;
};

struct blecon_event_loop_t* blecon_zephyr_event_loop_new(void) {
    static const struct blecon_event_loop_fn_t event_loop_fn = {
        .setup = blecon_zephyr_event_loop_setup,
        .run = blecon_zephyr_event_loop_run,
        .register_event = blecon_zephyr_event_loop_register_event,
        .lock = blecon_zephyr_event_loop_lock,
        .unlock = blecon_zephyr_event_loop_unlock,
        .signal = blecon_zephyr_event_loop_signal,
    };

    struct blecon_zephyr_event_loop_t* zephyr_event_loop = BLECON_ALLOC(sizeof(struct blecon_zephyr_event_loop_t));
    if(zephyr_event_loop == NULL) {
        blecon_fatal_error();
    }

    blecon_event_loop_init(&zephyr_event_loop->event_loop, &event_loop_fn);

    k_mutex_init(&zephyr_event_loop->z_mutex);
    k_event_init(&zephyr_event_loop->z_event);

    zephyr_event_loop->events_count = 0;

    return &zephyr_event_loop->event_loop;
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
        uint32_t event_mask = BLECON_EVENT_BREAK | ~(UINT32_MAX << zephyr_event_loop->events_count);

        // Wait for events
        uint32_t events = k_event_wait(&zephyr_event_loop->z_event, event_mask, false, K_FOREVER);

        // Clear raised events (not done automatically by k_event_wait())
        k_event_set_masked(&zephyr_event_loop->z_event, 0, events);

        // Call any user event callback
        for(size_t p = 0; p < zephyr_event_loop->events_count; p++) {
            k_mutex_lock(&zephyr_event_loop->z_mutex, K_FOREVER);
            if( events & (1u << p) ) {
                blecon_event_on_raised(&zephyr_event_loop->events[p].event);
            }
            k_mutex_unlock(&zephyr_event_loop->z_mutex);
        }

        // If the break event was raised, return from loop
        if( events & BLECON_EVENT_BREAK ) {
            return;
        }
    }
}

struct blecon_event_t* blecon_zephyr_event_loop_register_event(struct blecon_event_loop_t* event_loop) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;
    blecon_assert(zephyr_event_loop->events_count < BLECON_ZEPHYR_EVENT_LOOP_MAX_EVENTS);

    struct blecon_zephyr_event_t* zephyr_event = &zephyr_event_loop->events[zephyr_event_loop->events_count];
    zephyr_event_loop->events_count++;

    return &zephyr_event->event;
}

void blecon_zephyr_event_loop_lock(struct blecon_event_loop_t* event_loop) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;
    k_mutex_lock(&zephyr_event_loop->z_mutex, K_FOREVER);
}

void blecon_zephyr_event_loop_unlock(struct blecon_event_loop_t* event_loop) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;
    k_mutex_unlock(&zephyr_event_loop->z_mutex);
}

void blecon_zephyr_event_loop_signal(struct blecon_event_loop_t* event_loop, struct blecon_event_t* event) {
    struct blecon_zephyr_event_loop_t* zephyr_event_loop = (struct blecon_zephyr_event_loop_t*) event_loop;
    struct blecon_zephyr_event_t* zephyr_event = (struct blecon_zephyr_event_t*) event;

    // Retrieve event id based on position within the array
    size_t event_id = (size_t)(zephyr_event - &zephyr_event_loop->events[0]);

    k_event_post(&zephyr_event_loop->z_event, 1u << event_id);
}

