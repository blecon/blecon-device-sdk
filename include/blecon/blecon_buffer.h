/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

struct blecon_buffer_t {
    uint8_t* data;
    size_t sz;
    void* underlying_mem;
    size_t underlying_sz;
};

struct blecon_buffer_t blecon_buffer_alloc(size_t sz);
void blecon_buffer_free(struct blecon_buffer_t buffer);

static inline bool blecon_buffer_is_valid(struct blecon_buffer_t buffer) {
    return buffer.underlying_mem != NULL;
}

static inline struct blecon_buffer_t blecon_buffer_get_null(void) {
    struct blecon_buffer_t buffer = {.data = NULL, .sz = 0, .underlying_mem = NULL};
    return buffer;
}

struct blecon_buffer_t blecon_buffer_stack(struct blecon_buffer_t buffer, size_t header_sz, size_t footer_sz);
struct blecon_buffer_t blecon_buffer_unstack(struct blecon_buffer_t buffer, size_t header_sz, size_t footer_sz);

size_t blecon_buffer_total_allocations_size(void);
size_t blecon_buffer_total_allocations_count(void);


#ifdef __cplusplus
}
#endif
