/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "stddef.h"

// UUIDs
#define BLECON_UUID_SZ 16u
#define BLECON_UUID_STR_SZ (BLECON_UUID_SZ * 2 + 4 + 1) // 4 dashes in string representation and null terminator

// NeoBeacon ID
#define BLECON_NEOBEACON_ID_SZ 6u
#define BLECON_NEOBEACON_ID_STR_SZ (BLECON_NEOBEACON_ID_SZ * 2 + 1) // Hex string representation with null terminator
#define BLECON_NEOBEACON_V8_ID_ASSIGNMENT_PROOF_SZ 32

// Blecon URNs have the following format:
#define BLECON_URN_UUID_PREFIX "urn:uuid:"
#define BLECON_URN_UUID_SZ (sizeof(BLECON_URN_UUID_PREFIX) - 1 + BLECON_UUID_STR_SZ)
#define BLECON_URN_NEOBEACON_PREFIX "urn:neobeacon:"
#define BLECON_URN_NEOBEACON_ID_SZ (sizeof(BLECON_URN_NEOBEACON_PREFIX) - 1 + BLECON_NEOBEACON_ID_STR_SZ - 1 + 1 /* Null terminator */)
#define BLECON_URN_MAX_SZ ((BLECON_URN_UUID_SZ > BLECON_URN_NEOBEACON_ID_SZ) ? BLECON_URN_UUID_SZ : BLECON_URN_NEOBEACON_ID_SZ)

// Blecon device URLs have the following format:
// https://blecon.dev/urn:uuid:[UUID]
// https://bcn.id/neo/[NEOBEACON_ID]
#define BLECON_URL_UUID_PREFIX "https://blecon.dev/urn:uuid:"
#define BLECON_URL_UUID_SZ ((sizeof(BLECON_URL_UUID_PREFIX) - 1) + (BLECON_UUID_STR_SZ - 1) /* UUID excluding null terminator */ \
+ 1 /* Null terminator */)
#define BLECON_URL_NEOBEACON_PREFIX "https://bcn.id/neo/"
#define BLECON_URL_NEOBEACON_SZ ((sizeof(BLECON_URL_NEOBEACON_PREFIX) - 1) + (BLECON_NEOBEACON_ID_STR_SZ - 1) /* ID excluding null terminator */ \
+ 1 /* Null terminator */)
#define BLECON_URL_MAX_SZ ((BLECON_URL_UUID_SZ > BLECON_URL_NEOBEACON_SZ) ? BLECON_URL_UUID_SZ : BLECON_URL_NEOBEACON_SZ)
#define BLECON_URL_UNPROVISIONED "https://bcn.id/unprovisioned"

// Device-stack tracing span/trace id; 0 is "no span". Here rather than in the tracing
// headers because the public request headers carry one and the SDK ships no tracing header.
typedef size_t blecon_tracing_id_t;

// Model Id
#define BLECON_APPLICATION_MODEL_ID_SZ 16

// MTU
#define BLECON_MTU 4352u

// NVM
#define BLECON_NVM_DATA_SZ 4096 // A page size on nRF52
#define BLECON_NVM_WRITE_ALIGNMENT 16 // Granularity of a blecon_nvm_write() offset/size

// Bluetooth
#define BLECON_BLUETOOTH_ADDR_SZ 6
// The Bluetooth Core specification reserves 127 (0x7F) for "RSSI not available" in
// advertising reports and HCI Read RSSI. Every port passes it through as-is, so an
// int8_t RSSI anywhere in the library may hold it: compare against it before treating
// the value as a measurement. The hotspot never sends it to the service, the scanner
// drops it whenever an RSSI threshold is configured (it cannot satisfy one) and passes
// it only when filtering is off, and the survey never averages it.
#define BLECON_BLUETOOTH_RSSI_UNKNOWN 0x7F
// Scanner RSSI threshold that disables RSSI filtering. -127 dBm is the lowest RSSI the
// specification allows, so no measured report is ever below it; an unknown RSSI is
// only dropped when a real threshold is set.
#define BLECON_BLUETOOTH_RSSI_THRESHOLD_NONE (-127)
#define BLECON_MAX_ADVERTISING_SETS 3
#define BLECON_L2CAP_MPS 247                    // Assuming DLE are supported, this is as much as we can send in a PDU (251 bytes - 4-byte L2CAP headers)
#define BLECON_L2CAP_MTU (BLECON_L2CAP_MPS - 2) // This is how much we can send in a SDU so that it's not fragmented over multiple PDUs
#define BLECON_L2CAP_MAX_CONNECTIONS 2
#define BLECON_L2CAP_MAX_QUEUED_TX_BUFFERS 2
#define BLECON_L2CAP_MAX_QUEUED_RX_BUFFERS 2

// Crypto
#define BLECON_X25519_PRIVATE_KEY_SZ 32
#define BLECON_X25519_PUBLIC_KEY_SZ 32
#define BLECON_X25519_OUT_SECRET_SZ 32
#define BLECON_SHA256_HASH_SZ 32
#define BLECON_CHACHA20_POLY1305_SECRET_SZ 32
#define BLECON_CHACHA20_POLY1305_TAG_SZ 16
#define BLECON_CHACHA20_POLY1305_NONCE_SZ 12

// GAP
// Allow compile-time override via -DBLECON_BLUETOOTH_SERVICE_16UUID_VAL=0xNNNN
#ifndef BLECON_BLUETOOTH_SERVICE_16UUID_VAL
#define BLECON_BLUETOOTH_SERVICE_16UUID_VAL 0xFD0D
#endif

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
