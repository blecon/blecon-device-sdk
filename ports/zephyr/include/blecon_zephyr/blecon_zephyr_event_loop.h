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

typedef void (*blecon_zephyr_event_callback_t)(struct blecon_event_loop_t* event_loop, void* user_data);

uint32_t blecon_zephyr_event_loop_assign_event(struct blecon_event_loop_t* event_loop, blecon_zephyr_event_callback_t callback, void* user_data);

void blecon_zephyr_event_loop_post_event(struct blecon_event_loop_t* event_loop, uint32_t event_id);

void blecon_zephyr_event_loop_break(struct blecon_event_loop_t* event_loop);

#ifdef __cplusplus
}
#endif
