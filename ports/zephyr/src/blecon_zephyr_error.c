/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stdlib.h"
#include "string.h"

// When NDEBUG is defined, assert() does nothing
// so undefine it explicitly
#ifdef NDEBUG
#undef NDEBUG
#endif

#include "assert.h"

void blecon_fatal_error(void) {
    assert(0);
}