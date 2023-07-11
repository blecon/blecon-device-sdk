/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stddef.h"
#include "string.h"
#include "stdlib.h"

char* strdup(const char* str) {
    size_t sz = strlen(str) + 1;
    char* str2 = malloc(sz);
    memcpy(str2, str, sz); // include zero-terminator
    return str2;
}
