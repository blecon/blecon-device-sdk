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
#include "blecon/port/blecon_nvm.h"

struct blecon_nvm_t* blecon_zephyr_nvm_init(void);

#ifdef __cplusplus
}
#endif
