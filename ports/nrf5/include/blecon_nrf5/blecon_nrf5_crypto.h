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
#include "blecon/port/blecon_crypto.h"

struct blecon_crypto_t* blecon_nrf5_crypto_init(void);
struct blecon_crypto_t* blecon_nrf5_crypto_internal_get(void);

#ifdef __cplusplus
}
#endif
