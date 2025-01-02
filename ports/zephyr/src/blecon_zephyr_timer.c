/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stdlib.h"
#include "string.h"

#include "blecon_zephyr_timer.h"

#include "blecon/blecon_memory.h"
#include "blecon/blecon_error.h"

#include "zephyr/kernel.h"

struct blecon_zephyr_timer_t;

static void blecon_zephyr_timer_setup(struct blecon_timer_t* timer);
static uint64_t blecon_zephyr_timer_get_monotonic_time(struct blecon_timer_t* timer);
static void blecon_zephyr_timer_set_timeout(struct blecon_timer_t* timer, uint32_t timeout_ms);
static void blecon_zephyr_timer_cancel_timeout(struct blecon_timer_t* timer);

static void blecon_zephyr_timer_on_timeout(struct k_timer* timer);

struct blecon_zephyr_timer_t {
    struct blecon_timer_t timer;
    struct k_timer z_timer;
};

struct blecon_timer_t* blecon_zephyr_timer_new(void) {
    static const struct blecon_timer_fn_t timer_fn = {
        .setup = blecon_zephyr_timer_setup,
        .get_monotonic_time = blecon_zephyr_timer_get_monotonic_time,
        .set_timeout = blecon_zephyr_timer_set_timeout,
        .cancel_timeout = blecon_zephyr_timer_cancel_timeout,
    };

    struct blecon_zephyr_timer_t* zephyr_timer = BLECON_ALLOC(sizeof(struct blecon_zephyr_timer_t));
    if(zephyr_timer == NULL) {
        blecon_fatal_error();
    }

    blecon_timer_init(&zephyr_timer->timer, &timer_fn);

    k_timer_init(&zephyr_timer->z_timer, blecon_zephyr_timer_on_timeout, NULL);

    return &zephyr_timer->timer;
}

void blecon_zephyr_timer_setup(struct blecon_timer_t* timer) {
    struct blecon_zephyr_timer_t* zephyr_timer = (struct blecon_zephyr_timer_t*) timer;
    (void)zephyr_timer;
}

uint64_t blecon_zephyr_timer_get_monotonic_time(struct blecon_timer_t* timer) {
    struct blecon_zephyr_timer_t* zephyr_timer = (struct blecon_zephyr_timer_t*) timer;
    (void)zephyr_timer;

    return (uint64_t)k_uptime_get();
}

void blecon_zephyr_timer_set_timeout(struct blecon_timer_t* timer, uint32_t timeout_ms) {
    struct blecon_zephyr_timer_t* zephyr_timer = (struct blecon_zephyr_timer_t*) timer;

    k_timer_start(&zephyr_timer->z_timer, K_MSEC(timeout_ms), K_FOREVER);
}

void blecon_zephyr_timer_cancel_timeout(struct blecon_timer_t* timer) {
    struct blecon_zephyr_timer_t* zephyr_timer = (struct blecon_zephyr_timer_t*) timer;

    k_timer_stop(&zephyr_timer->z_timer);
}

void blecon_zephyr_timer_on_timeout(struct k_timer* timer) {
    struct blecon_zephyr_timer_t* zephyr_timer 
        = (struct blecon_zephyr_timer_t*) ((char*)timer - offsetof(struct blecon_zephyr_timer_t, z_timer));

    blecon_timer_on_timeout(&zephyr_timer->timer);
}
