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
typedef void (*blecon_event_loop_callback_t)(struct blecon_event_loop_t* event_loop, bool timeout, void* user_data);

struct blecon_event_loop_fn_t {
    void (*setup)(struct blecon_event_loop_t* event_loop);
    void (*run)(struct blecon_event_loop_t* event_loop);
    uint64_t (*get_monotonic_time)(struct blecon_event_loop_t* event_loop);
    void (*set_timeout)(struct blecon_event_loop_t* event_loop, uint32_t timeout_ms);
    void (*cancel_timeout)(struct blecon_event_loop_t* event_loop);
    void (*signal)(struct blecon_event_loop_t* event_loop);
    void (*lock)(struct blecon_event_loop_t* event_loop);
    void (*unlock)(struct blecon_event_loop_t* event_loop);
};

struct blecon_event_loop_t {
    const struct blecon_event_loop_fn_t* fns;
    blecon_event_loop_callback_t callback;
    void* callback_user_data;
};

static inline void blecon_event_loop_init(struct blecon_event_loop_t* event_loop, const struct blecon_event_loop_fn_t* fns) {
    event_loop->fns = fns;
    event_loop->callback = NULL;
    event_loop->callback_user_data = NULL;
}

static inline void blecon_event_loop_setup(struct blecon_event_loop_t* event_loop, blecon_event_loop_callback_t callback, void* user_data) {
    event_loop->callback = callback;
    event_loop->callback_user_data = user_data;
    event_loop->fns->setup(event_loop);
}

static inline void blecon_event_loop_run(struct blecon_event_loop_t* event_loop) {
    event_loop->fns->run(event_loop);
}

static inline uint64_t blecon_event_loop_get_monotonic_time(struct blecon_event_loop_t* event_loop) {
    return event_loop->fns->get_monotonic_time(event_loop);
}

static inline void blecon_event_loop_set_timeout(struct blecon_event_loop_t* event_loop, uint32_t timeout_ms) {
    event_loop->fns->set_timeout(event_loop, timeout_ms);
}

static inline void blecon_event_loop_cancel_timeout(struct blecon_event_loop_t* event_loop) {
    event_loop->fns->cancel_timeout(event_loop);
}

static inline void blecon_event_loop_signal(struct blecon_event_loop_t* event_loop) {
    event_loop->fns->signal(event_loop);
}

static inline void blecon_event_loop_lock(struct blecon_event_loop_t* event_loop) {
    event_loop->fns->lock(event_loop);
}

static inline void blecon_event_loop_unlock(struct blecon_event_loop_t* event_loop) {
    event_loop->fns->unlock(event_loop);
}

static inline void blecon_event_loop_on_event(struct blecon_event_loop_t* event_loop, bool timeout) {
    event_loop->callback(event_loop, timeout, event_loop->callback_user_data);
}

#ifdef __cplusplus
}
#endif
