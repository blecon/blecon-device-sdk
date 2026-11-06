/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "string.h"
#include "blecon_zephyr_otp.h"
#include "blecon/blecon_defs.h"
#include "blecon/blecon_error.h"
#include "blecon/port/blecon_otp.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

/*
 * The OTP region is described by a single `blecon_otp` devicetree node, but
 * it can be wired up two different ways depending on the target:
 *
 *   1. Memory-mapped OTP (default on real chips). The node is at the top of
 *      the devicetree with `compatible = "blecon,otp"` and a `reg = <addr size>`
 *      pointing at a chip-specific address that is directly readable.
 *
 *   2. Flash-backed OTP (for native_sim and any platform whose OTP image lives
 *      in a flash partition). The node sits as a fixed-partition child of a
 *      flash controller's `partitions` block; `reg = <offset size>` is then
 *      a flash offset and the data is read via `flash_read()` on first use.
 *
 * The two modes are auto-detected here at compile time. Why both: the
 * memory-mapped path is what real hardware needs (no flash driver indirection,
 * zero RAM footprint); the flash-backed path is required on native_sim because
 * its `mock_flash` buffer is heap-allocated, so the DT `reg` value is not a
 * valid host pointer and direct dereference would read random process memory.
 */

static bool blecon_zephyr_otp_get_key_info(enum blecon_otp_key_t key, size_t* offset, size_t* size);
static bool blecon_zephyr_otp_is_blank(const uint8_t* data, size_t size);
static bool blecon_zephyr_otp_hal_has_key(struct blecon_otp_t* otp, enum blecon_otp_key_t key);
static bool blecon_zephyr_otp_hal_get(struct blecon_otp_t* otp, enum blecon_otp_key_t key, uint8_t* buffer, size_t buffer_size);

bool blecon_zephyr_otp_get_key_info(enum blecon_otp_key_t key, size_t* offset, size_t* size) {
    switch( key ) {
#if DT_NODE_HAS_PROP(DT_NODELABEL(blecon_otp), neobeacon_v8_id_offset)
    case blecon_otp_key_neobeacon_v8_unique_id:
        *offset = DT_PROP(DT_NODELABEL(blecon_otp), neobeacon_v8_id_offset);
        *size = BLECON_NEOBEACON_ID_SZ;
        return true;
#endif
#if DT_NODE_HAS_PROP(DT_NODELABEL(blecon_otp), neobeacon_v8_id_assignment_proof_offset)
    case blecon_otp_key_neobeacon_v8_id_assignment_proof:
        *offset = DT_PROP(DT_NODELABEL(blecon_otp), neobeacon_v8_id_assignment_proof_offset);
        *size = BLECON_NEOBEACON_V8_ID_ASSIGNMENT_PROOF_SZ;
        return true;
#endif
    default:
        return false;
    }
}

#if DT_NODE_EXISTS(DT_NODELABEL(blecon_otp))

/*
 * Mode selection. A `blecon_otp` node sitting inside a `fixed-partitions`
 * block (e.g. `&flash0/partitions/...` on native_sim) means the OTP image
 * lives in flash and must be loaded via the flash driver; otherwise the
 * `reg` value is taken to be a directly-readable address.
 */
#if DT_NODE_HAS_COMPAT(DT_PARENT(DT_NODELABEL(blecon_otp)), fixed_partitions)
#define BLECON_OTP_FROM_PARTITION 1
#else
#define BLECON_OTP_FROM_PARTITION 0
#endif

#if BLECON_OTP_FROM_PARTITION

#define BLECON_OTP_FLASH_DEVICE PARTITION_DEVICE(blecon_otp)
#define BLECON_OTP_FLASH_OFFSET PARTITION_OFFSET(blecon_otp)
#define BLECON_OTP_SIZE         PARTITION_SIZE(blecon_otp)

/*
 * Cached copy of the OTP region. Loaded lazily on first access via
 * flash_read(); afterwards reads are served from RAM. OTP is write-once so a
 * single load is sufficient — there is no invalidation path. The cache also
 * lets has_key()/get() share one flash transaction across multiple keys.
 */
static uint8_t blecon_zephyr_otp_buffer[BLECON_OTP_SIZE];
static bool blecon_zephyr_otp_loaded = false;

static const uint8_t* blecon_zephyr_otp_data(void) {
    if( !blecon_zephyr_otp_loaded ) {
        const struct device* dev = BLECON_OTP_FLASH_DEVICE;
        blecon_assert(device_is_ready(dev));
        int rc = flash_read(dev, BLECON_OTP_FLASH_OFFSET, blecon_zephyr_otp_buffer, BLECON_OTP_SIZE);
        // A flash read failure on a configured OTP partition is a platform
        // misconfiguration — fail loudly rather than silently degrade.
        blecon_assert(rc == 0);
        blecon_zephyr_otp_loaded = true;
    }
    return blecon_zephyr_otp_buffer;
}

#else /* memory-mapped */

#define BLECON_OTP_ADDR DT_REG_ADDR(DT_NODELABEL(blecon_otp))
#define BLECON_OTP_SIZE DT_REG_SIZE(DT_NODELABEL(blecon_otp))

static const uint8_t* blecon_zephyr_otp_data(void) {
    return (const uint8_t*) BLECON_OTP_ADDR;
}

#endif /* BLECON_OTP_FROM_PARTITION */

bool blecon_zephyr_otp_has_key(enum blecon_otp_key_t key) {
    size_t offset, size;
    if( !blecon_zephyr_otp_get_key_info(key, &offset, &size) ) {
        return false;
    }

    if( offset + size > BLECON_OTP_SIZE ) {
        return false;
    }

    const uint8_t* data = blecon_zephyr_otp_data() + offset;
    return !blecon_zephyr_otp_is_blank(data, size);
}

bool blecon_zephyr_otp_get(enum blecon_otp_key_t key, uint8_t* buffer, size_t buffer_size) {
    size_t offset, size;
    if( !blecon_zephyr_otp_get_key_info(key, &offset, &size) ) {
        return false;
    }

    if( buffer_size != size ) {
        return false;
    }

    if( !blecon_zephyr_otp_has_key(key) ) {
        return false;
    }

    memcpy(buffer, blecon_zephyr_otp_data() + offset, size);
    return true;
}

#else /* no DT node */

bool blecon_zephyr_otp_has_key(enum blecon_otp_key_t key) {
    (void)key;
    return false;
}

bool blecon_zephyr_otp_get(enum blecon_otp_key_t key, uint8_t* buffer, size_t buffer_size) {
    (void)key;
    (void)buffer;
    (void)buffer_size;
    return false;
}

#endif

bool blecon_zephyr_otp_is_blank(const uint8_t* data, size_t size) {
    for( size_t i = 0; i < size; i++ ) {
        if( data[i] != 0xff ) {
            return false;
        }
    }
    return true;
}

struct blecon_otp_t* blecon_zephyr_otp_init(void) {
    static const struct blecon_otp_fn_t otp_fn = {
        .has_key = blecon_zephyr_otp_hal_has_key,
        .get = blecon_zephyr_otp_hal_get,
    };

    static struct blecon_otp_t otp;
    blecon_otp_init(&otp, &otp_fn);
    return &otp;
}

bool blecon_zephyr_otp_hal_has_key(struct blecon_otp_t* otp, enum blecon_otp_key_t key) {
    (void)otp;
    return blecon_zephyr_otp_has_key(key);
}

bool blecon_zephyr_otp_hal_get(struct blecon_otp_t* otp, enum blecon_otp_key_t key, uint8_t* buffer, size_t buffer_size) {
    (void)otp;
    return blecon_zephyr_otp_get(key, buffer, buffer_size);
}
