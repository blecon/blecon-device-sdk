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
#define BLECON_URL_PREFIX "https://blecon.dev/urn:uuid:"

#define BLECON_URL_SZ ((sizeof(BLECON_URL_PREFIX) - 1) \
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
#define BLECON_L2CAP_MAX_QUEUED_RX_BUFFERS 2

// Crypto
#define BLECON_X25519_PRIVATE_KEY_SZ        32
#define BLECON_X25519_PUBLIC_KEY_SZ         32
#define BLECON_X25519_OUT_SECRET_SZ         32
#define BLECON_SHA256_HASH_SZ               32
#define BLECON_CHACHA20_POLY1305_SECRET_SZ  32
#define BLECON_CHACHA20_POLY1305_TAG_SZ     16
#define BLECON_CHACHA20_POLY1305_NONCE_SZ   12

// GAP
#define BLECON_BLUETOOTH_SERVICE_16UUID_VAL 0xFD0D

// Base UUID for GATT characteristics
#define BLECON_BASE_GATT_SERVICE_UUID 0xec, 0x4f, 0x00, 0x00, 0x15, 0x37, 0x44, 0x3e, 0xb0, 0x5a, 0x71, 0x30, 0x51, 0x21, 0x2f, 0x1c

// Macro to generate a UUID for a GATT characteristic
#define BLECON_GATT_CHARACTERISTIC_UUID(idx) 0xec, 0x4f, ((idx) >> 8u) & 0xffu, (idx) & 0xffu, 0x15, 0x37, 0x44, 0x3e, 0xb0, 0x5a, 0x71, 0x30, 0x51, 0x21, 0x2f, 0x1c

// Maximum namespace size
#define BLECON_NAMESPACE_MAX_SZ 32 // Including null terminator

// Maximum method size
#define BLECON_METHOD_MAX_SZ 32 // Including null terminator

// Maximum content type size
#define BLECON_CONTENT_TYPE_MAX_SZ 32 // Including null terminator
