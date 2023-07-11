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
#include "blecon/port/blecon_nfc.h"

struct blecon_nfc_t* blecon_zephyr_nfc_init(void);

#ifdef __cplusplus
}
#endif
