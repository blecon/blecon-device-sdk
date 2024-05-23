/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "assert.h"

#include "blecon_zephyr_nfc.h"
#include "blecon/blecon_memory.h"

#if CONFIG_BLECON_PORT_NFC
#include <nfc_t2t_lib.h>
#include <nfc/ndef/msg.h>
#include <nfc/ndef/uri_msg.h>

static void nfc_callback(void* context, nfc_t2t_event_t event, const uint8_t* data, size_t data_length);
#endif

static void blecon_zephyr_nfc_setup(struct blecon_nfc_t* nfc);
static void blecon_zephyr_nfc_set_message(struct blecon_nfc_t* nfc, const uint8_t* nfc_data, size_t nfc_data_sz);
static void blecon_zephyr_nfc_start(struct blecon_nfc_t* nfc);
static void blecon_zephyr_nfc_stop(struct blecon_nfc_t* nfc);

#if CONFIG_BLECON_PORT_NFC
#define URI_PREFIX "https://"

struct blecon_zephyr_nfc_t {
    struct blecon_nfc_t nfc;
    uint8_t ndef_buffer[256];
    bool nfc_active;
};

struct blecon_nfc_t* blecon_zephyr_nfc_init(void) {
    static const struct blecon_nfc_fn_t nfc_fn = {
        .setup = blecon_zephyr_nfc_setup,
        .set_message = blecon_zephyr_nfc_set_message,
        .start = blecon_zephyr_nfc_start,
        .stop = blecon_zephyr_nfc_stop
    };

    struct blecon_zephyr_nfc_t* zephyr_nfc = BLECON_ALLOC(sizeof(struct blecon_zephyr_nfc_t));
    if(zephyr_nfc == NULL) {
        blecon_fatal_error();
    }

    blecon_nfc_init(&zephyr_nfc->nfc, &nfc_fn);

    memset(zephyr_nfc->ndef_buffer, 0, sizeof(zephyr_nfc->ndef_buffer));
    zephyr_nfc->nfc_active = false;

    return &zephyr_nfc->nfc;
}

void blecon_zephyr_nfc_setup(struct blecon_nfc_t* nfc) {
    int ret = nfc_t2t_setup(nfc_callback, NULL);
    if(ret) {
        blecon_fatal_error();
    }
}

void blecon_zephyr_nfc_set_message(struct blecon_nfc_t* nfc, const uint8_t* nfc_data, size_t nfc_data_sz) {
    struct blecon_zephyr_nfc_t* zephyr_nfc = (struct blecon_zephyr_nfc_t*) nfc;

    bool nfc_active = zephyr_nfc->nfc_active;
    if(nfc_active) {
        blecon_zephyr_nfc_stop(nfc);
    }

    // For now only support URIs starting with https://
    if((nfc_data_sz < strlen(URI_PREFIX)) 
        || !!memcmp(nfc_data, URI_PREFIX, strlen(URI_PREFIX))) {
        blecon_fatal_error();
    }

    nfc_data += strlen(URI_PREFIX);
    nfc_data_sz -= strlen(URI_PREFIX);
    
    // Encode message
    uint32_t len = sizeof(zephyr_nfc->ndef_buffer);
    int ret = nfc_ndef_uri_msg_encode(NFC_URI_HTTPS, nfc_data, nfc_data_sz,
        zephyr_nfc->ndef_buffer, &len);
    if(ret) {
        blecon_fatal_error();
    }

    // Set payload
    ret = nfc_t2t_payload_set(zephyr_nfc->ndef_buffer, len);
    if(ret) {
        blecon_fatal_error();
    }

    if(nfc_active) {
        blecon_zephyr_nfc_start(nfc);
    }
}

void blecon_zephyr_nfc_start(struct blecon_nfc_t* nfc) {
    struct blecon_zephyr_nfc_t* zephyr_nfc = (struct blecon_zephyr_nfc_t*) nfc;

    int ret = nfc_t2t_emulation_start();
    if(ret) {
        blecon_fatal_error();
    }

    zephyr_nfc->nfc_active = true;
}

void blecon_zephyr_nfc_stop(struct blecon_nfc_t* nfc) {
    struct blecon_zephyr_nfc_t* zephyr_nfc = (struct blecon_zephyr_nfc_t*) nfc;

    int ret = nfc_t2t_emulation_stop();
    if(ret) {
        blecon_fatal_error();
    }

    zephyr_nfc->nfc_active = false;
}

static void nfc_callback(void* context, nfc_t2t_event_t event, const uint8_t* data, size_t data_length) {
}

#else
struct blecon_nfc_t* blecon_zephyr_nfc_init(void) {
    static const struct blecon_nfc_fn_t nfc_fn = {
        .setup = blecon_zephyr_nfc_setup,
        .set_message = blecon_zephyr_nfc_set_message,
        .start = blecon_zephyr_nfc_start,
        .stop = blecon_zephyr_nfc_stop
    };

    struct blecon_nfc_t* nfc = BLECON_ALLOC(sizeof(struct blecon_nfc_t));
    if(nfc == NULL) {
        blecon_fatal_error();
    }

    blecon_nfc_init(nfc, &nfc_fn);

    return nfc;
}

void blecon_zephyr_nfc_setup(struct blecon_nfc_t* nfc) {
}

void blecon_zephyr_nfc_set_message(struct blecon_nfc_t* nfc, const uint8_t* nfc_data, size_t nfc_data_sz) {
}

void blecon_zephyr_nfc_start(struct blecon_nfc_t* nfc) {
}

void blecon_zephyr_nfc_stop(struct blecon_nfc_t* nfc) {
}

#endif