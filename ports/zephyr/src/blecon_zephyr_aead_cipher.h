/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "blecon/port/blecon_crypto.h"
#include "blecon/blecon_defs.h"

#include "psa/crypto.h"

struct blecon_zephyr_aead_cipher_t {
    struct blecon_crypto_aead_cipher_t cipher;
    psa_key_handle_t key_handle;
};

#ifdef __cplusplus
}
#endif
