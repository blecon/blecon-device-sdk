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

static void blecon_zephyr_nfc_setup(struct blecon_nfc_t* nfc);
static void blecon_zephyr_nfc_set_message(struct blecon_nfc_t* nfc, const uint8_t* nfc_data, size_t nfc_data_sz);
static void blecon_zephyr_nfc_start(struct blecon_nfc_t* nfc);
static void blecon_zephyr_nfc_stop(struct blecon_nfc_t* nfc);

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
    char* nfc_url = malloc(nfc_data_sz + 1);
    memcpy(nfc_url, nfc_data, nfc_data_sz);
    nfc_url[nfc_data_sz] = '\0';
    assert(nfc_url != NULL);
    printf("NFC URL: %s\r\n", nfc_url);
    free(nfc_url);
}

void blecon_zephyr_nfc_start(struct blecon_nfc_t* nfc) {

}

void blecon_zephyr_nfc_stop(struct blecon_nfc_t* nfc) {

}
