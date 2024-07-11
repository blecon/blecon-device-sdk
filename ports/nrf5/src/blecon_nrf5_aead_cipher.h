/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "blecon/port/blecon_crypto.h"
#include "nrf_crypto.h"

struct blecon_nrf5_aead_cipher_t {
    struct blecon_crypto_aead_cipher_t cipher;
    nrf_crypto_aead_context_t ctx;
};

#ifdef __cplusplus
}
#endif
