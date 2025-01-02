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

#include "blecon_event_loop.h"

// Timer
struct blecon_timer_t;

struct blecon_timer_fn_t {
    void (*setup)(struct blecon_timer_t* timer);
    uint64_t (*get_monotonic_time)(struct blecon_timer_t* timer);
    void (*set_timeout)(struct blecon_timer_t* timer, uint32_t timeout_ms);
    void (*cancel_timeout)(struct blecon_timer_t* timer);
};

struct blecon_timer_t {
    const struct blecon_timer_fn_t* fns;
    struct blecon_event_t* event;
};

static inline void blecon_timer_init(struct blecon_timer_t* timer, const struct blecon_timer_fn_t* fns) {
    timer->fns = fns;
    timer->event = NULL;
}

static inline void blecon_timer_setup(struct blecon_timer_t* timer, struct blecon_event_t* event) {
    timer->event = event;
    timer->fns->setup(timer);
}

static inline uint64_t blecon_timer_get_monotonic_time(struct blecon_timer_t* timer) {
    return timer->fns->get_monotonic_time(timer);
}

static inline void blecon_timer_set_timeout(struct blecon_timer_t* timer, uint32_t timeout_ms) {
    timer->fns->set_timeout(timer, timeout_ms);
}

static inline void blecon_timer_cancel_timeout(struct blecon_timer_t* timer) {
    timer->fns->cancel_timeout(timer);
}

static inline void blecon_timer_on_timeout(struct blecon_timer_t* timer) {
    blecon_event_signal(timer->event);
}

static inline struct blecon_event_t* blecon_timer_get_event(struct blecon_timer_t* timer) {
    return timer->event;
}

#ifdef __cplusplus
}
#endif
