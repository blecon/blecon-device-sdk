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

// NFC
struct blecon_nfc_t;
struct blecon_nfc_fn_t {
    void (*setup)(struct blecon_nfc_t* nfc);
    void (*set_message)(struct blecon_nfc_t* nfc, const uint8_t* nfc_data, size_t nfc_data_sz); // TODO consider pre-encoding the message
    void (*start)(struct blecon_nfc_t* nfc);
    void (*stop)(struct blecon_nfc_t* nfc);
};

struct blecon_nfc_t {
    const struct blecon_nfc_fn_t* fns;
};

static inline void blecon_nfc_init(struct blecon_nfc_t* nfc, const struct blecon_nfc_fn_t* fns) {
    nfc->fns = fns;
}

static inline void blecon_nfc_setup(struct blecon_nfc_t* nfc) {
    nfc->fns->setup(nfc);
}

static inline void blecon_nfc_set_message(struct blecon_nfc_t* nfc, const uint8_t* nfc_data, size_t nfc_data_sz) {
    nfc->fns->set_message(nfc, nfc_data, nfc_data_sz);
}

static inline void blecon_nfc_start(struct blecon_nfc_t* nfc) {
    nfc->fns->start(nfc);
}

static inline void blecon_nfc_stop(struct blecon_nfc_t* nfc) {
    nfc->fns->stop(nfc);
}

#ifdef __cplusplus
}
#endif
