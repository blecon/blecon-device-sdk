/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"

// Errors
#if BLECON_CUSTOM_ERROR_HANDLER == 1
#include "blecon_custom_error_handler.h"
#else
void blecon_fatal_error(void);
#endif

static inline void blecon_assert(bool condition) {
    if(!condition) {
        blecon_fatal_error();
    }
}

#ifdef __cplusplus
}
#endif
