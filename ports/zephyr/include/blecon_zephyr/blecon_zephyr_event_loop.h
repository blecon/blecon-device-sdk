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
#include "blecon/port/blecon_event_loop.h"

struct blecon_event_loop_t* blecon_zephyr_event_loop_new(void);

void blecon_zephyr_event_loop_break(struct blecon_event_loop_t* event_loop);

#ifdef __cplusplus
}
#endif
