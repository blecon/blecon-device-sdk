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
#include "blecon/port/blecon_otp.h"

// Returns a blecon_otp_t instance backed by the Zephyr OTP implementation.
struct blecon_otp_t* blecon_zephyr_otp_init(void);

// Returns true if the key exists and contains valid (non-0xff) data
bool blecon_zephyr_otp_has_key(enum blecon_otp_key_t key);

// Reads the value for the given key into buffer.
// buffer_size must match the expected size for the key.
// Returns true on success, false if key not present, OTP not available, or wrong buffer size.
bool blecon_zephyr_otp_get(enum blecon_otp_key_t key, uint8_t* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif
