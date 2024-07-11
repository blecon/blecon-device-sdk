/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blecon_nrf5_nfc.h"

#include "nfc_t2t_lib.h"
#include "nfc_uri_msg.h"

#include "string.h"

#include "sdk_config.h"

#if NORDIC_TARGET_SUPPORTS_NFC
// Validate nRF5 SDK Config
#if !NFC_NDEF_MSG_ENABLED || !NFC_NDEF_RECORD_ENABLED || !NFC_NDEF_URI_MSG_ENABLED || !NFC_NDEF_URI_REC_ENABLED \
    || !NFC_PLATFORM_ENABLED
#error "NFC_NDEF_MSG_ENABLED, NFC_NDEF_RECORD_ENABLED, NFC_NDEF_URI_MSG_ENABLED, NFC_NDEF_URI_REC_ENABLED and NFC_PLATFORM_ENABLED must be enabled in sdk_config.h"
#endif
#endif

static void blecon_nrf5_nfc_setup(struct blecon_nfc_t* nfc);
static void blecon_nrf5_nfc_set_message(struct blecon_nfc_t* nfc, const uint8_t* nfc_data, size_t nfc_data_sz);
static void blecon_nrf5_nfc_start(struct blecon_nfc_t* nfc);
static void blecon_nrf5_nfc_stop(struct blecon_nfc_t* nfc);

static struct blecon_nfc_t _nfc;

struct blecon_nfc_t* blecon_nrf5_nfc_init(void) {
    static const struct blecon_nfc_fn_t nfc_fn = {
        .setup = blecon_nrf5_nfc_setup,
        .set_message = blecon_nrf5_nfc_set_message,
        .start = blecon_nrf5_nfc_start,
        .stop = blecon_nrf5_nfc_stop
    };

    blecon_nfc_init(&_nfc, &nfc_fn);

    return &_nfc;
}

#if NORDIC_TARGET_SUPPORTS_NFC

static void nfc_callback(void * p_context, nfc_t2t_event_t event, const uint8_t * p_data, size_t data_length);
static uint8_t _ndef_buffer[256];
static bool _nfc_active;

#define URI_PREFIX "https://"

void blecon_nrf5_nfc_setup(struct blecon_nfc_t* nfc) {
    uint32_t ret = nfc_t2t_setup(nfc_callback, NULL);
    if(ret != NRF_SUCCESS) {
        blecon_fatal_error();
    }

    _nfc_active = false;
}

void blecon_nrf5_nfc_set_message(struct blecon_nfc_t* nfc, const uint8_t* nfc_data, size_t nfc_data_sz) {
    bool nfc_active = _nfc_active;
    if(nfc_active) {
        blecon_nrf5_nfc_stop(nfc);
    }

    // For now only support URIs starting with https://
    if((nfc_data_sz < strlen(URI_PREFIX)) 
        || !!memcmp(nfc_data, URI_PREFIX, strlen(URI_PREFIX))) {
        blecon_fatal_error();
    }

    nfc_data += strlen(URI_PREFIX);
    nfc_data_sz -= strlen(URI_PREFIX);
    
    // Encode message
    uint32_t len = sizeof(_ndef_buffer);
    uint32_t ret = nfc_uri_msg_encode(NFC_URI_HTTPS, nfc_data, nfc_data_sz,
        _ndef_buffer, &len);
    if(ret != NRF_SUCCESS) {
        blecon_fatal_error();
    }

    // Set payload
    ret = nfc_t2t_payload_set(_ndef_buffer, len);
    if(ret != NRF_SUCCESS) {
        blecon_fatal_error();
    }

    if(nfc_active) {
        blecon_nrf5_nfc_start(&_nfc);
    }
}

void blecon_nrf5_nfc_start(struct blecon_nfc_t* nfc) {
    uint32_t ret = nfc_t2t_emulation_start();
    if(ret != NRF_SUCCESS) {
        blecon_fatal_error();
    }

    _nfc_active = true;
}

void blecon_nrf5_nfc_stop(struct blecon_nfc_t* nfc) {
    uint32_t ret = nfc_t2t_emulation_stop();
    if(ret != NRF_SUCCESS) {
        blecon_fatal_error();
    }

    _nfc_active = false;
}

// Private functions
void nfc_callback(void * p_context, nfc_t2t_event_t event, const uint8_t * p_data, size_t data_length) {
    (void)p_context;
    (void)event;
    (void)p_data;
    (void)data_length;
}

#else

void blecon_nrf5_nfc_setup(struct blecon_nfc_t* nfc) {}
void blecon_nrf5_nfc_set_message(struct blecon_nfc_t* nfc, const uint8_t* nfc_data, size_t nfc_data_sz) {}
void blecon_nrf5_nfc_start(struct blecon_nfc_t* nfc) {}
void blecon_nrf5_nfc_stop(struct blecon_nfc_t* nfc) {}

#endif
