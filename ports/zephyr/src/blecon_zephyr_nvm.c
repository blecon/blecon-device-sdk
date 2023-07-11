/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "assert.h"

#include "blecon_zephyr_nvm.h"
#include "blecon/blecon_defs.h"
#include "blecon/blecon_error.h"
#include "blecon/blecon_memory.h"

#include "zephyr/device.h"
#include "zephyr/drivers/flash.h"
#include "zephyr/storage/flash_map.h"
#include "zephyr/devicetree.h"

#define BLECON_NVM_FLASH_AREA_LABEL     blecon
#define BLECON_NVM_FLASH_AREA_DEVICE    FIXED_PARTITION_DEVICE(BLECON_NVM_FLASH_AREA_LABEL)
#define BLECON_NVM_FLASH_AREA_OFFSET    FIXED_PARTITION_OFFSET(BLECON_NVM_FLASH_AREA_LABEL)
#define BLECON_NVM_FLASH_AREA_ADDR      FIXED_PARTITION_DATA_FIELD(BLECON_NVM_FLASH_AREA_LABEL, _ADDRESS)
#define BLECON_NVM_FLASH_AREA_SIZE      FIXED_PARTITION_SIZE(BLECON_NVM_FLASH_AREA_LABEL)
#define BLECON_NVM_FLASH_AREA_ID        FIXED_PARTITION_ID(BLECON_NVM_FLASH_AREA_LABEL)
#define BLECON_NVM_FLASH_CONTROLLER     DT_GPARENT(BLECON_NVM_FLASH_AREA_ID)

#if DT_NODE_HAS_PROP(BLECON_NVM_FLASH_CONTROLLER, write_block_size)
#define BLECON_NVM_FLASH_BLOCK_SIZE \
	DT_PROP(BLECON_NVM_FLASH_CONTROLLER, write_block_size)

BUILD_ASSERT((BLECON_NVM_DATA_SZ % BLECON_NVM_FLASH_BLOCK_SIZE == 0),
	     "BLECON_NVM_DATA_SZ is not a multiple of "
	     "BLECON_NVM_FLASH_BLOCK_SIZE");
#endif

BUILD_ASSERT( (BLECON_NVM_FLASH_AREA_SIZE >= BLECON_NVM_DATA_SZ), 
                "Not enough allocated NVM space");

struct blecon_zephyr_nvm_t {
    struct blecon_nvm_t nvm;
};

static void blecon_zephyr_nvm_setup(struct blecon_nvm_t* nvm);
static bool blecon_zephyr_nvm_is_free(struct blecon_nvm_t* nvm);
static void* blecon_zephyr_nvm_address(struct blecon_nvm_t* nvm);
static bool blecon_zephyr_nvm_write(struct blecon_nvm_t* nvm, const uint8_t* data, size_t data_sz);
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
    if(zephyr_nvm == NULL) {
        blecon_fatal_error();
    }

    blecon_nvm_init(&zephyr_nvm->nvm, &nvm_fn);

    return &zephyr_nvm->nvm;
}

void blecon_zephyr_nvm_setup(struct blecon_nvm_t* nvm) {
    struct blecon_zephyr_nvm_t* zephyr_nvm = (struct blecon_zephyr_nvm_t*) nvm;
    (void) zephyr_nvm;

    blecon_assert(device_is_ready(BLECON_NVM_FLASH_AREA_DEVICE));
}

bool blecon_zephyr_nvm_is_free(struct blecon_nvm_t* nvm) {
    struct blecon_zephyr_nvm_t* zephyr_nvm = (struct blecon_zephyr_nvm_t*) nvm;
    (void) zephyr_nvm;

    for(const uint32_t* word = (const uint32_t*)BLECON_NVM_FLASH_AREA_ADDR; 
        word < (const uint32_t*)BLECON_NVM_FLASH_AREA_ADDR + (BLECON_NVM_DATA_SZ / sizeof(uint32_t)); word++) {
        if(*word != 0xFFFFFFFFul) {
            return false;
        }
    }

    return true;

    return false;
}

void* blecon_zephyr_nvm_address(struct blecon_nvm_t* nvm) {
    struct blecon_zephyr_nvm_t* zephyr_nvm = (struct blecon_zephyr_nvm_t*) nvm;
    (void) zephyr_nvm;

    return (void*)BLECON_NVM_FLASH_AREA_ADDR;
}

bool blecon_zephyr_nvm_write(struct blecon_nvm_t* nvm, const uint8_t* data, size_t data_sz) {
    struct blecon_zephyr_nvm_t* zephyr_nvm = (struct blecon_zephyr_nvm_t*) nvm;
    (void) zephyr_nvm;

    int ret = flash_write(BLECON_NVM_FLASH_AREA_DEVICE, BLECON_NVM_FLASH_AREA_OFFSET, data, data_sz);
    if(ret) {
		return false;
	}

    return true;
}

void blecon_zephyr_nvm_protect(struct blecon_nvm_t* nvm) {
    struct blecon_zephyr_nvm_t* zephyr_nvm = (struct blecon_zephyr_nvm_t*) nvm;
    (void) zephyr_nvm;

    // No-op for now
}

bool blecon_zephyr_nvm_erase(struct blecon_nvm_t* nvm) {
    struct blecon_zephyr_nvm_t* zephyr_nvm = (struct blecon_zephyr_nvm_t*) nvm;
    (void) zephyr_nvm;
    
    int ret = flash_erase(BLECON_NVM_FLASH_AREA_DEVICE, BLECON_NVM_FLASH_AREA_OFFSET, BLECON_NVM_DATA_SZ);
    if(ret) {
		return false;
	}

    return true;
}
