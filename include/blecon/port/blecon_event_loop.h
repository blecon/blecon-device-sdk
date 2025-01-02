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

// Event loop
struct blecon_event_loop_t;
struct blecon_event_t;

struct blecon_event_loop_fn_t {
    void (*setup)(struct blecon_event_loop_t* event_loop);
    struct blecon_event_t* (*register_event)(struct blecon_event_loop_t* event_loop);
    void (*run)(struct blecon_event_loop_t* event_loop);
    void (*lock)(struct blecon_event_loop_t* event_loop);
    void (*unlock)(struct blecon_event_loop_t* event_loop);
    void (*signal)(struct blecon_event_loop_t* event_loop, struct blecon_event_t* event);
};

struct blecon_event_loop_t {
    const struct blecon_event_loop_fn_t* fns;
};

typedef void (*blecon_event_callback_t)(struct blecon_event_t* event, void* user_data);
struct blecon_event_t {
    struct blecon_event_loop_t* event_loop;
    blecon_event_callback_t callback;
    void* callback_user_data;
};

static inline void blecon_event_loop_init(struct blecon_event_loop_t* event_loop, const struct blecon_event_loop_fn_t* fns) {
    event_loop->fns = fns;
}

static inline void blecon_event_loop_setup(struct blecon_event_loop_t* event_loop) {
    event_loop->fns->setup(event_loop);
}

static inline struct blecon_event_t* blecon_event_loop_register_event(struct blecon_event_loop_t* event_loop, blecon_event_callback_t callback, void* callback_user_data) {
    struct blecon_event_t* event = event_loop->fns->register_event(event_loop);
    event->event_loop = event_loop;
    event->callback = callback;
    event->callback_user_data = callback_user_data;
    return event;
}

static inline void blecon_event_loop_run(struct blecon_event_loop_t* event_loop) {
    event_loop->fns->run(event_loop);
}

static inline void blecon_event_loop_lock(struct blecon_event_loop_t* event_loop) {
    event_loop->fns->lock(event_loop);
}

static inline void blecon_event_loop_unlock(struct blecon_event_loop_t* event_loop) {
    event_loop->fns->unlock(event_loop);
}

// Should be safe to call from ISR/different thread/*nix signal - and should raise blecon_event_on_raised() in the main loop
static inline void blecon_event_signal(struct blecon_event_t* event) {
    event->event_loop->fns->signal(event->event_loop, event);
}

static inline void blecon_event_on_raised(struct blecon_event_t* event) {
    event->callback(event, event->callback_user_data);
}

#ifdef __cplusplus
}
#endif
