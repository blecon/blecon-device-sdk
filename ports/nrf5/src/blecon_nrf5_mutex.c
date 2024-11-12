/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stdlib.h"
#include "string.h"

#include "blecon_nrf5/blecon_nrf5_mutex.h"

struct blecon_mutex_t* blecon_mutex_new(void) {
    return NULL;
}

void blecon_mutex_lock(struct blecon_mutex_t* mutex) {
    (void)mutex;
}

void blecon_mutex_unlock(struct blecon_mutex_t* mutex) {
    (void)mutex;
}

void blecon_mutex_free(struct blecon_mutex_t* mutex) {
    (void)mutex;
}

