/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "blecon_zephyr_nvm.h"
#include "blecon/blecon_defs.h"
#include "blecon/blecon_error.h"
#include "blecon/blecon_memory.h"

#include "zephyr/device.h"
#include "zephyr/drivers/flash.h"
#include "zephyr/storage/flash_map.h"
#include "zephyr/devicetree.h"

/*
 * NVM Flash Area Configuration
 *
 * Three approaches are supported:
 * 1. Nordic Partition Manager (PM) - for Nordic devices with sysbuild
 * 2. Device Tree Fixed Partition - for native_sim and other non-PM platforms
 * 3. Fallback error for unsupported configurations
 *
 * For native_sim, flash is emulated via a file and cannot be memory-mapped,
 * so we use a RAM buffer to cache NVM contents and return that from address().
 */

#if defined(CONFIG_PARTITION_MANAGER_ENABLED) && CONFIG_PARTITION_MANAGER_ENABLED
/* Nordic Partition Manager - supports XIP (execute-in-place) */
#include "pm_config.h"

#define BLECON_NVM_FLASH_AREA_DEVICE DEVICE_DT_GET(DT_NODELABEL(PM_BLECON_DEV))
#define BLECON_NVM_FLASH_AREA_OFFSET PM_BLECON_OFFSET
#define BLECON_NVM_FLASH_AREA_ADDR PM_BLECON_ADDRESS
#define BLECON_NVM_FLASH_AREA_SIZE PM_BLECON_SIZE
#define BLECON_NVM_FLASH_CONTROLLER PM_BLECON_DEV
#define BLECON_NVM_SUPPORTS_XIP 1

#elif DT_NODE_EXISTS(DT_NODELABEL(blecon_partition))
/* Device Tree Fixed Partition - for native_sim and other non-PM platforms */
/* Flash emulated via file - use RAM buffer for NVM caching */

#define BLECON_NVM_FLASH_AREA_DEVICE PARTITION_DEVICE(blecon_partition)
#define BLECON_NVM_FLASH_AREA_OFFSET PARTITION_OFFSET(blecon_partition)
#define BLECON_NVM_FLASH_AREA_SIZE PARTITION_SIZE(blecon_partition)
#define BLECON_NVM_SUPPORTS_XIP 0

#else
/* Unsupported configuration */
#if defined(CONFIG_SOC_FAMILY_NORDIC_NRF)
#error "The Partition Manager must be enabled for Nordic devices (nRF52/53/54): build with sysbuild and set SB_CONFIG_PARTITION_MANAGER=y in the application's sysbuild.conf (it is off by default since NCS 3.4)."
#else
#error "NVM flash area not configured. Define 'blecon_partition' in device tree overlay."
#endif
#endif

/* Block size validation for PM-based configurations */
#if BLECON_NVM_SUPPORTS_XIP && DT_NODE_HAS_PROP(BLECON_NVM_FLASH_CONTROLLER, write_block_size)
#define BLECON_NVM_FLASH_BLOCK_SIZE \
    DT_PROP(BLECON_NVM_FLASH_CONTROLLER, write_block_size)

BUILD_ASSERT((BLECON_NVM_DATA_SZ % BLECON_NVM_FLASH_BLOCK_SIZE == 0),
             "BLECON_NVM_DATA_SZ is not a multiple of "
             "BLECON_NVM_FLASH_BLOCK_SIZE");
#endif

BUILD_ASSERT((BLECON_NVM_FLASH_AREA_SIZE >= BLECON_NVM_DATA_SZ), "Not enough allocated NVM space");

struct blecon_zephyr_nvm_t {
    struct blecon_nvm_t nvm;
#if !BLECON_NVM_SUPPORTS_XIP
    /* RAM buffer for non-XIP platforms (e.g., native_sim) */
    uint8_t buffer[BLECON_NVM_DATA_SZ];
#endif
};

static void blecon_zephyr_nvm_setup(struct blecon_nvm_t* nvm);
static bool blecon_zephyr_nvm_is_free(struct blecon_nvm_t* nvm);
static void* blecon_zephyr_nvm_address(struct blecon_nvm_t* nvm);
static bool blecon_zephyr_nvm_write(struct blecon_nvm_t* nvm, size_t offset, const uint8_t* data, size_t data_sz);
static void blecon_zephyr_nvm_protect(struct blecon_nvm_t* nvm);
static bool blecon_zephyr_nvm_erase(struct blecon_nvm_t* nvm);

struct blecon_nvm_t* blecon_zephyr_nvm_init(void) {
    static const struct blecon_nvm_fn_t nvm_fn = {
        .setup = blecon_zephyr_nvm_setup,
        .is_free = blecon_zephyr_nvm_is_free,
        .address = blecon_zephyr_nvm_address,
        .write = blecon_zephyr_nvm_write,
        .protect = blecon_zephyr_nvm_protect,
        .erase = blecon_zephyr_nvm_erase
    };

    struct blecon_zephyr_nvm_t* zephyr_nvm = BLECON_ALLOC(sizeof(struct blecon_zephyr_nvm_t));
    if( zephyr_nvm == NULL ) {
        blecon_fatal_error();
    }

    blecon_nvm_init(&zephyr_nvm->nvm, &nvm_fn);

    return &zephyr_nvm->nvm;
}

void blecon_zephyr_nvm_setup(struct blecon_nvm_t* nvm) {
    struct blecon_zephyr_nvm_t* zephyr_nvm = (struct blecon_zephyr_nvm_t*) nvm;

    blecon_assert(device_is_ready(BLECON_NVM_FLASH_AREA_DEVICE));

#if CONFIG_BLECON_PORT_NVM_ERASE
    blecon_zephyr_nvm_erase(nvm);
#endif

#if !BLECON_NVM_SUPPORTS_XIP
    /* For non-XIP platforms, load flash contents into RAM buffer */
    int ret = flash_read(BLECON_NVM_FLASH_AREA_DEVICE,
                         BLECON_NVM_FLASH_AREA_OFFSET,
                         zephyr_nvm->buffer,
                         BLECON_NVM_DATA_SZ);
    blecon_assert(ret == 0);
#else
    (void) zephyr_nvm;
#endif
}

bool blecon_zephyr_nvm_is_free(struct blecon_nvm_t* nvm) {
    struct blecon_zephyr_nvm_t* zephyr_nvm = (struct blecon_zephyr_nvm_t*) nvm;

#if BLECON_NVM_SUPPORTS_XIP
    /* Direct memory access for XIP platforms */
    (void) zephyr_nvm;
    for( const uint32_t* word = (const uint32_t*) BLECON_NVM_FLASH_AREA_ADDR;
         word < (const uint32_t*) BLECON_NVM_FLASH_AREA_ADDR + (BLECON_NVM_DATA_SZ / sizeof(uint32_t)); word++ ) {
        if( *word != 0xFFFFFFFFul ) {
            return false;
        }
    }
#else
    /* Check RAM buffer for non-XIP platforms */
    for( size_t i = 0; i < BLECON_NVM_DATA_SZ; i += sizeof(uint32_t) ) {
        uint32_t word;
        memcpy(&word, &zephyr_nvm->buffer[i], sizeof(uint32_t));
        if( word != 0xFFFFFFFFul ) {
            return false;
        }
    }
#endif

    return true;
}

void* blecon_zephyr_nvm_address(struct blecon_nvm_t* nvm) {
    struct blecon_zephyr_nvm_t* zephyr_nvm = (struct blecon_zephyr_nvm_t*) nvm;

#if BLECON_NVM_SUPPORTS_XIP
    /* Direct memory-mapped flash for XIP platforms */
    (void) zephyr_nvm;
    return (void*) BLECON_NVM_FLASH_AREA_ADDR;
#else
    /* Return RAM buffer pointer for non-XIP platforms */
    return (void*) zephyr_nvm->buffer;
#endif
}

bool blecon_zephyr_nvm_write(struct blecon_nvm_t* nvm, size_t offset, const uint8_t* data, size_t data_sz) {
    struct blecon_zephyr_nvm_t* zephyr_nvm = (struct blecon_zephyr_nvm_t*) nvm;

    blecon_assert(offset <= BLECON_NVM_DATA_SZ);
    blecon_assert(data_sz <= BLECON_NVM_DATA_SZ - offset);

    int ret = flash_write(BLECON_NVM_FLASH_AREA_DEVICE, BLECON_NVM_FLASH_AREA_OFFSET + offset, data, data_sz);
    if( ret ) {
        return false;
    }

#if !BLECON_NVM_SUPPORTS_XIP
    memcpy(zephyr_nvm->buffer + offset, data, data_sz);
#else
    (void) zephyr_nvm;
#endif

    return true;
}

void blecon_zephyr_nvm_protect(struct blecon_nvm_t* nvm) {
    struct blecon_zephyr_nvm_t* zephyr_nvm = (struct blecon_zephyr_nvm_t*) nvm;
    (void) zephyr_nvm;

    // No-op for now
}

bool blecon_zephyr_nvm_erase(struct blecon_nvm_t* nvm) {
    struct blecon_zephyr_nvm_t* zephyr_nvm = (struct blecon_zephyr_nvm_t*) nvm;

    int ret = flash_erase(BLECON_NVM_FLASH_AREA_DEVICE, BLECON_NVM_FLASH_AREA_OFFSET, BLECON_NVM_DATA_SZ);
    if( ret ) {
        return false;
    }

#if !BLECON_NVM_SUPPORTS_XIP
    /* Fill RAM buffer with 0xFF (erased state) for non-XIP platforms */
    memset(zephyr_nvm->buffer, 0xFF, BLECON_NVM_DATA_SZ);
#else
    (void) zephyr_nvm;
#endif

    return true;
}
