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
#include "blecon/blecon_defs.h"
#include "blecon/blecon_error.h"
#include "blecon/blecon_memory.h"

// Memfault
struct blecon_memfault_t;
struct blecon_memfault_fn_t {
    void (*set_device_identifier)(struct blecon_memfault_t* memfault, const char* device_identifier);
    bool (*get_next_chunk)(struct blecon_memfault_t* memfault);
    bool (*get_chunk_data)(struct blecon_memfault_t* memfault, uint8_t* buf, size_t* sz);
};

struct blecon_memfault_t {
    const struct blecon_memfault_fn_t* fns;
};

static inline void blecon_memfault_init(struct blecon_memfault_t* memfault, const struct blecon_memfault_fn_t* fns) {
    memfault->fns = fns;
}

static inline void blecon_memfault_set_device_identifier(struct blecon_memfault_t* memfault, const char* device_identifier) {
    memfault->fns->set_device_identifier(memfault, device_identifier);
}

static inline bool blecon_memfault_get_next_chunk(struct blecon_memfault_t* memfault) {
    return memfault->fns->get_next_chunk(memfault);
}

static inline bool blecon_memfault_get_chunk_data(struct blecon_memfault_t* memfault, uint8_t* buf, size_t* sz) {
    return memfault->fns->get_chunk_data(memfault, buf, sz);
}

#ifdef __cplusplus
}
#endif
