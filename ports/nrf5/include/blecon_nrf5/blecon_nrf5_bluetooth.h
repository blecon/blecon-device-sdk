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
#include "blecon/port/blecon_bluetooth.h"

struct blecon_bluetooth_t* blecon_nrf5_bluetooth_init(void);

#ifdef __cplusplus
}
#endif
