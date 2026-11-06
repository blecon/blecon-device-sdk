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

enum blecon_otp_key_t {
    blecon_otp_key_neobeacon_v8_unique_id,
    blecon_otp_key_neobeacon_v8_id_assignment_proof,
};

struct blecon_otp_t;
struct blecon_otp_fn_t {
    bool (*has_key)(struct blecon_otp_t* otp, enum blecon_otp_key_t key);
    bool (*get)(struct blecon_otp_t* otp, enum blecon_otp_key_t key, uint8_t* buffer, size_t buffer_size);
};

struct blecon_otp_t {
    const struct blecon_otp_fn_t* fns;
};

static inline void blecon_otp_init(struct blecon_otp_t* otp, const struct blecon_otp_fn_t* fns) {
    otp->fns = fns;
}

static inline bool blecon_otp_has_key(struct blecon_otp_t* otp, enum blecon_otp_key_t key) {
    return otp->fns->has_key(otp, key);
}

static inline bool blecon_otp_get(struct blecon_otp_t* otp, enum blecon_otp_key_t key, uint8_t* buffer, size_t buffer_size) {
    return otp->fns->get(otp, key, buffer, buffer_size);
}

#ifdef __cplusplus
}
#endif
