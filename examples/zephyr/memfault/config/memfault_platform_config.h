/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Platform overrides for the default configuration settings in the memfault-firmware-sdk.
//! Default configuration settings can be found in "memfault/config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEMFAULT_USE_GNU_BUILD_ID 1
#define MEMFAULT_COMPACT_LOG_ENABLE 0
#define MEMFAULT_PLATFORM_BOOT_TIMER_CUSTOM 0
#define MEMFAULT_METRICS_HEARTBEAT_INTERVAL_SECS (15*60) // Heartbeat every 15 minutes
#define MEMFAULT_COREDUMP_COLLECT_HEAP_STATS 1
#define MEMFAULT_COREDUMP_COLLECT_LOG_REGIONS 1 // Collect logs with coredumps
#define MEMFAULT_METRICS_SYNC_SUCCESS 1 // Enable collection of sync success metrics for device vitals

#ifdef __cplusplus
}
#endif
