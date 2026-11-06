/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blecon_nrf5_nvm.h"
#include "blecon/blecon_error.h"

#include "nrf.h"
#include "nrfx_nvmc.h"
#include "string.h"
#include "stdint.h"

static void blecon_nrf5_nvm_setup(struct blecon_nvm_t* nvm);
static bool blecon_nrf5_nvm_is_free(struct blecon_nvm_t* nvm);
static void* blecon_nrf5_nvm_address(struct blecon_nvm_t* nvm);
static bool blecon_nrf5_nvm_write(struct blecon_nvm_t* nvm, size_t offset, const uint8_t* data, size_t data_sz);
static void blecon_nrf5_nvm_protect(struct blecon_nvm_t* nvm);
static bool blecon_nrf5_nvm_erase(struct blecon_nvm_t* nvm);

static uint32_t _nvm_config[0x1000 / 4] __attribute__((section(".blecon_config"))) __attribute__((used));
static struct blecon_nvm_t _nvm;

struct blecon_nvm_t* blecon_nrf5_nvm_init(void) {
    static const struct blecon_nvm_fn_t nvm_fn = {
        .setup = blecon_nrf5_nvm_setup,
        .is_free = blecon_nrf5_nvm_is_free,
        .address = blecon_nrf5_nvm_address,
        .write = blecon_nrf5_nvm_write,
        .protect = blecon_nrf5_nvm_protect,
        .erase = blecon_nrf5_nvm_erase
    };

    blecon_nvm_init(&_nvm, &nvm_fn);

    return &_nvm;
}

void blecon_nrf5_nvm_setup(struct blecon_nvm_t* nvm) {
}

bool blecon_nrf5_nvm_is_free(struct blecon_nvm_t* nvm) {
    for( const uint32_t* word = (const uint32_t*) &_nvm_config; word < (const uint32_t*) &_nvm_config + (sizeof(_nvm_config) / sizeof(uint32_t)); word++ ) {
        if( *word != 0xFFFFFFFF ) {
            return false;
        }
    }

    return true;
}

void* blecon_nrf5_nvm_address(struct blecon_nvm_t* nvm) {
    return (void*) _nvm_config;
}

bool blecon_nrf5_nvm_write(struct blecon_nvm_t* nvm, size_t offset, const uint8_t* data, size_t data_sz) {
    if( offset > sizeof(_nvm_config) || data_sz > sizeof(_nvm_config) - offset ) {
        blecon_fatal_error(); // TODO
        return false;
    }

    nrfx_nvmc_bytes_write((uint32_t) ((uintptr_t) _nvm_config + offset), data, data_sz);

    // nrfx_nvmc_bytes_write() waits for NVMC readiness ahead of each word write,
    // not after the last one, so it can return with the final word still in
    // flight. Callers read the region straight back out of flash via
    // blecon_nrf5_nvm_address() — blecon_identity_store() hashes it — so wait
    // for the write to land rather than relying on the flash controller
    // stalling the read.
    while( !nrfx_nvmc_write_done_check() ) {}

    return true;
}

void blecon_nrf5_nvm_protect(struct blecon_nvm_t* nvm) {
    // No-op for now
}

bool blecon_nrf5_nvm_erase(struct blecon_nvm_t* nvm) {
    if( nrfx_nvmc_page_erase((uint32_t) _nvm_config) != NRFX_SUCCESS ) {
        return false;
    }

    return true;
}