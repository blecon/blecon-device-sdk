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
#include "blecon/port/blecon_timer.h"

struct blecon_timer_t* blecon_zephyr_timer_new(void);
// Not with CONFIG_TRACING_OBJECT_TRACKING, whose list of timers has no removal: a fatal error there
void blecon_zephyr_timer_free(struct blecon_timer_t* timer);

#ifdef __cplusplus
}
#endif
