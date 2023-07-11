/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stddef.h"
#include "blecon_buffer.h"

struct blecon_bearer_t;

struct blecon_bearer_fn_t {
    struct blecon_buffer_t (*alloc)(struct blecon_bearer_t* bearer, size_t sz, void* user_data);
    size_t (*mtu)(struct blecon_bearer_t* bearer, void* user_data);
    void (*send)(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf, void* user_data);
    void (*close)(struct blecon_bearer_t* bearer, void* user_data);
};

struct blecon_bearer_callbacks_t {
    void (*on_open)(struct blecon_bearer_t* bearer, void* user_data);
    void (*on_received)(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf, void* user_data);
    void (*on_sent)(struct blecon_bearer_t* bearer, void* user_data);
    void (*on_closed)(struct blecon_bearer_t* bearer, void* user_data);
};

struct blecon_bearer_t {
    const struct blecon_bearer_fn_t* fns;
    void* fns_user_data;
    const struct blecon_bearer_callbacks_t* callbacks;
    void* callbacks_user_data;
};

// Set by implementation
static inline void blecon_bearer_set_callbacks(struct blecon_bearer_t* bearer, const struct blecon_bearer_callbacks_t* callbacks, void* callbacks_user_data) {
    bearer->callbacks = callbacks;
    bearer->callbacks_user_data = callbacks_user_data;
}

static inline struct blecon_buffer_t blecon_bearer_alloc(struct blecon_bearer_t* bearer, size_t sz) {
    return bearer->fns->alloc(bearer, sz, bearer->fns_user_data);
}

static inline void blecon_bearer_free(struct blecon_buffer_t buf) {
    blecon_buffer_free(buf);
}

static inline size_t blecon_bearer_mtu(struct blecon_bearer_t* bearer) {
    return bearer->fns->mtu(bearer, bearer->fns_user_data);
}

static inline void blecon_bearer_send(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf) {
    bearer->fns->send(bearer, buf, bearer->fns_user_data);
}

static inline void blecon_bearer_close(struct blecon_bearer_t* bearer) {
    bearer->fns->close(bearer, bearer->fns_user_data);
}

// Called by implementation
void blecon_bearer_set_functions(struct blecon_bearer_t* bearer, const struct blecon_bearer_fn_t* functions, void* functions_user_data);
void blecon_bearer_on_open(struct blecon_bearer_t* bearer);
void blecon_bearer_on_received(struct blecon_bearer_t* bearer, struct blecon_buffer_t buf);
void blecon_bearer_on_sent(struct blecon_bearer_t* bearer);
void blecon_bearer_on_closed(struct blecon_bearer_t* bearer);

#ifdef __cplusplus
}
#endif
