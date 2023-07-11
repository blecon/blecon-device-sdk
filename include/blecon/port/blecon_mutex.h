/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct blecon_mutex_t;

struct blecon_mutex_t* blecon_mutex_new(void);

void blecon_mutex_lock(struct blecon_mutex_t* mutex);

void blecon_mutex_unlock(struct blecon_mutex_t* mutex);

void blecon_mutex_free(struct blecon_mutex_t* mutex);

#ifdef __cplusplus
}
#endif
