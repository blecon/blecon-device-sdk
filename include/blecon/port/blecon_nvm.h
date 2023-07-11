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

// Flash
struct blecon_nvm_t;
struct blecon_nvm_fn_t {
    void (*setup)(struct blecon_nvm_t* nvm);
    bool (*is_free)(struct blecon_nvm_t* nvm);
    void* (*address)(struct blecon_nvm_t* nvm);
    bool (*write)(struct blecon_nvm_t* nvm, const uint8_t* data, size_t data_sz);
    void (*protect)(struct blecon_nvm_t* nvm);
    bool (*erase)(struct blecon_nvm_t* nvm);
};

struct blecon_nvm_t {
    const struct blecon_nvm_fn_t* fns;
};

static inline void blecon_nvm_init(struct blecon_nvm_t* nvm, const struct blecon_nvm_fn_t* fns) {
    nvm->fns = fns;
}

static inline void blecon_nvm_setup(struct blecon_nvm_t* nvm) {
    nvm->fns->setup(nvm);
}

static inline bool blecon_nvm_is_free(struct blecon_nvm_t* nvm) {
    return nvm->fns->is_free(nvm);
}

static inline void* blecon_nvm_address(struct blecon_nvm_t* nvm) {
    return nvm->fns->address(nvm);
}

static inline bool blecon_nvm_write(struct blecon_nvm_t* nvm, const uint8_t* data, size_t data_sz) {
    return nvm->fns->write(nvm, data, data_sz);
}

static inline void blecon_nvm_protect(struct blecon_nvm_t* nvm) {
    nvm->fns->protect(nvm);
}

static inline bool blecon_nvm_erase(struct blecon_nvm_t* nvm) {
    return nvm->fns->erase(nvm);
}

#ifdef __cplusplus
}
#endif
