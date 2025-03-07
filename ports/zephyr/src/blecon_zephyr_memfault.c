/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <string.h>
#include "blecon/blecon_defs.h"
#include "blecon_zephyr/blecon_zephyr_memfault.h"

#include <memfault/components.h>
#include <memfault/metrics/metrics.h>

LOG_MODULE_REGISTER(memfault_integration);

static struct blecon_memfault_t _memfault;

static void memfault_set_device_identifier(struct blecon_memfault_t* memfault, const char* device_identifier);
static bool memfault_get_next_chunk(struct blecon_memfault_t* memfault);
static bool memfault_get_chunk_data(struct blecon_memfault_t* memfault, uint8_t* buf, size_t* sz);

static char memfault_device_uuid_str[BLECON_UUID_STR_SZ] = "";

struct blecon_memfault_t* blecon_zephyr_memfault_init() {
    static const struct blecon_memfault_fn_t memfault_fn = {
        .set_device_identifier = memfault_set_device_identifier,
        .get_next_chunk = memfault_get_next_chunk,
        .get_chunk_data = memfault_get_chunk_data
    };

    blecon_memfault_init(&_memfault, &memfault_fn);

    return &_memfault;
}

void memfault_set_device_identifier(struct blecon_memfault_t* memfault, const char* device_identifier) {
    strncpy(memfault_device_uuid_str, device_identifier, BLECON_UUID_STR_SZ);
    memfault_device_uuid_str[BLECON_UUID_STR_SZ - 1] = '\0';
}

bool memfault_get_next_chunk(struct blecon_memfault_t* memfault) {
    const sPacketizerConfig cfg = {
        // Enable multi packet chunking. This means a chunk may span multiple calls to
        // memfault_packetizer_get_next().
        .enable_multi_packet_chunk = true,
    };

    sPacketizerMetadata metadata;
    bool data_available = memfault_packetizer_begin(&cfg, &metadata);
    if (!data_available) {
        return false;
    }

    return true;
}

bool memfault_get_chunk_data(struct blecon_memfault_t* memfault, uint8_t* buf, size_t* sz) {
    eMemfaultPacketizerStatus packetizer_status = memfault_packetizer_get_next(buf, sz);
    blecon_assert(packetizer_status != kMemfaultPacketizerStatus_NoMoreData);

    if(packetizer_status == kMemfaultPacketizerStatus_EndOfChunk) {
        return false;
    }

    return true;
}

// Memfault integration
#if CONFIG_BLECON_MEMFAULT_DEFAULT_DEVICE_INFO
void memfault_platform_get_device_info(sMemfaultDeviceInfo *info) {
    info->device_serial = memfault_device_uuid_str;
    info->hardware_version = CONFIG_BLECON_MEMFAULT_HARDWARE_VERSION;
    info->software_version = CONFIG_BLECON_MEMFAULT_SOFTWARE_VERSION;
    info->software_type = CONFIG_BLECON_MEMFAULT_SOFTWARE_TYPE;
}
#endif /* CONFIG_BLECON_MEMFAULT_DEFAULT_DEVICE_INFO */