/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

// UUIDs
#define BLECON_UUID_SZ              16u
#define BLECON_UUID_STR_SZ (BLECON_UUID_SZ*2 + 4 + 1) // 4 dashes in string representation and null terminator

// Blecon device URLs have the following format:
// https://blecon.dev/[UUID]
#define BLECON_URL_PREFIX "https://blecon.dev/"

#define BLECON_URL_SZ (19 /* https://blecon.dev/ */ \
    + (BLECON_UUID_STR_SZ - 1) /* UUID excluding null terminator */ \
    + 1 /* Null terminator */)

// Model Id
#define BLECON_APPLICATION_MODEL_ID_SZ  16

// MTU
#define BLECON_MTU                  4352u

// NVM 
#define BLECON_NVM_DATA_SZ 4096 // A page size on nRF52

// Bluetooth 
#define BLECON_BLUETOOTH_ADDR_SZ 6
#define BLECON_MAX_ADVERTISING_SETS 3
#define BLECON_L2CAP_MPS 247 // Assuming DLE are supported, this is as much as we can send in a PDU (251 bytes - 4-byte L2CAP headers)
#define BLECON_L2CAP_MTU (BLECON_L2CAP_MPS - 2) // This is how much we can send in a SDU so that it's not fragmented over multiple PDUs
#define BLECON_L2CAP_MAX_CONNECTIONS 2
#define BLECON_L2CAP_MAX_QUEUED_TX_BUFFERS 2

// Crypto
#define BLECON_X25519_PRIVATE_KEY_SZ        32
#define BLECON_X25519_PUBLIC_KEY_SZ         32
#define BLECON_X25519_OUT_SECRET_SZ         32
#define BLECON_SHA256_HASH_SZ               32
#define BLECON_CHACHA20_POLY1305_SECRET_SZ  32
#define BLECON_CHACHA20_POLY1305_TAG_SZ     16
#define BLECON_CHACHA20_POLY1305_NONCE_SZ   12

