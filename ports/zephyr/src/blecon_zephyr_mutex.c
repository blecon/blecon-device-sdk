/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stdlib.h"
#include "string.h"

#include "blecon/blecon_memory.h"
#include "blecon/blecon_error.h"
#include "blecon_zephyr/blecon_zephyr_mutex.h"

#include "zephyr/kernel.h"

struct blecon_mutex_t {
    struct k_mutex mtx;
};

struct blecon_mutex_t* blecon_mutex_new(void) {
    struct blecon_mutex_t* mutex = BLECON_ALLOC(sizeof(struct blecon_mutex_t));
    if( mutex == NULL ) {
        blecon_fatal_error();
    }

    k_mutex_init(&mutex->mtx);

    return mutex;
}

void blecon_mutex_lock(struct blecon_mutex_t* mutex) {
    k_mutex_lock(&mutex->mtx, K_FOREVER);
}

void blecon_mutex_unlock(struct blecon_mutex_t* mutex) {
    k_mutex_unlock(&mutex->mtx);
}

void blecon_mutex_free(struct blecon_mutex_t* mutex) {
    free(mutex);
}

