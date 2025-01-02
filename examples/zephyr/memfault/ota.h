/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct blecon_event_loop_t;
struct blecon_t;

void ota_init(struct blecon_event_loop_t* event_loop, struct blecon_t* blecon, const char* request_namespace);

void ota_check_for_update(void);

#ifdef __cplusplus
}
#endif
