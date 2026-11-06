/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "string.h"
#include "blecon_nrf5_otp.h"
#include "blecon/blecon_defs.h"
#include "blecon/port/blecon_otp.h"

static bool blecon_nrf5_otp_hal_has_key(struct blecon_otp_t* otp, enum blecon_otp_key_t key);
static bool blecon_nrf5_otp_hal_get(struct blecon_otp_t* otp, enum blecon_otp_key_t key, uint8_t* buffer, size_t buffer_size);

#if defined(BLECON_OTP_PRESENT) && BLECON_OTP_PRESENT

static bool blecon_nrf5_otp_get_key_info(enum blecon_otp_key_t key, size_t* offset, size_t* size);
static bool blecon_nrf5_otp_is_blank(const uint8_t* data, size_t size);

bool blecon_nrf5_otp_get_key_info(enum blecon_otp_key_t key, size_t* offset, size_t* size) {
    switch( key ) {
#ifdef BLECON_OTP_NEOBEACON_V8_ID_OFFSET
    case blecon_otp_key_neobeacon_v8_unique_id:
        *offset = BLECON_OTP_NEOBEACON_V8_ID_OFFSET;
        *size = BLECON_NEOBEACON_ID_SZ;
        return true;
#endif
#ifdef BLECON_OTP_NEOBEACON_V8_ID_ASSIGNMENT_PROOF_OFFSET
    case blecon_otp_key_neobeacon_v8_id_assignment_proof:
        *offset = BLECON_OTP_NEOBEACON_V8_ID_ASSIGNMENT_PROOF_OFFSET;
        *size = BLECON_NEOBEACON_V8_ID_ASSIGNMENT_PROOF_SZ;
        return true;
#endif
    default:
        return false;
    }
}

bool blecon_nrf5_otp_has_key(enum blecon_otp_key_t key) {
    size_t offset, size;
    if( !blecon_nrf5_otp_get_key_info(key, &offset, &size) ) {
        return false;
    }

    if( offset + size > BLECON_OTP_SIZE ) {
        return false;
    }

    const uint8_t* data = (const uint8_t*)(uintptr_t)BLECON_OTP_ADDR + offset;
    return !blecon_nrf5_otp_is_blank(data, size);
}

bool blecon_nrf5_otp_get(enum blecon_otp_key_t key, uint8_t* buffer, size_t buffer_size) {
    size_t offset, size;
    if( !blecon_nrf5_otp_get_key_info(key, &offset, &size) ) {
        return false;
    }

    if( buffer_size != size ) {
        return false;
    }

    if( !blecon_nrf5_otp_has_key(key) ) {
        return false;
    }

    memcpy(buffer, (const uint8_t*)(uintptr_t)BLECON_OTP_ADDR + offset, size);
    return true;
}

bool blecon_nrf5_otp_is_blank(const uint8_t* data, size_t size) {
    for( size_t i = 0; i < size; i++ ) {
        if( data[i] != 0xff ) {
            return false;
        }
    }
    return true;
}

#else

bool blecon_nrf5_otp_has_key(enum blecon_otp_key_t key) {
    (void)key;
    return false;
}

bool blecon_nrf5_otp_get(enum blecon_otp_key_t key, uint8_t* buffer, size_t buffer_size) {
    (void)key;
    (void)buffer;
    (void)buffer_size;
    return false;
}

#endif

struct blecon_otp_t* blecon_nrf5_otp_init(void) {
    static const struct blecon_otp_fn_t otp_fn = {
        .has_key = blecon_nrf5_otp_hal_has_key,
        .get = blecon_nrf5_otp_hal_get,
    };

    static struct blecon_otp_t otp;
    blecon_otp_init(&otp, &otp_fn);
    return &otp;
}

bool blecon_nrf5_otp_hal_has_key(struct blecon_otp_t* otp, enum blecon_otp_key_t key) {
    (void)otp;
    return blecon_nrf5_otp_has_key(key);
}

bool blecon_nrf5_otp_hal_get(struct blecon_otp_t* otp, enum blecon_otp_key_t key, uint8_t* buffer, size_t buffer_size) {
    (void)otp;
    return blecon_nrf5_otp_get(key, buffer, buffer_size);
}
