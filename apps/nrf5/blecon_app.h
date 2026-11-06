/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "blecon/blecon.h"

void blecon_app_init(void);
void blecon_app_start(struct blecon_modem_t* modem);

#ifdef __cplusplus
}
#endif
