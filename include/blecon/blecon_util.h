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

// Swap endianness of a UUID
#define BLECON_UUID_ENDIANNESS_SWAP_2(u0, u1, u2, u3, u4, u5, u6, u7, u8, u9, ua, ub, uc, ud, ue, uf) \
    uf, ue, ud, uc, ub, ua, u9, u8, u7, u6, u5, u4, u3, u2, u1, u0
#define BLECON_UUID_ENDIANNESS_SWAP(uuid) BLECON_UUID_ENDIANNESS_SWAP_2(uuid)

char* blecon_util_append_hex_string(const uint8_t* data, size_t sz, char* output);
char* blecon_util_append_uuid_string(const uint8_t* data, char* output);

#ifdef __cplusplus
}
#endif
