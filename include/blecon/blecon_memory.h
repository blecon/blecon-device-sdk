/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdlib.h"

// Memory
#define BLECON_ALLOC(sz) malloc(sz)
#define BLECON_FREE(buf) free(buf)

#ifdef __cplusplus
}
#endif
