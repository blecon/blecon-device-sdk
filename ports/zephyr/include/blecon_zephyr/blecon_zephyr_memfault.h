/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "blecon/port/blecon_memfault.h"

struct blecon_memfault_t* blecon_zephyr_memfault_init();

#ifdef __cplusplus
}
#endif
