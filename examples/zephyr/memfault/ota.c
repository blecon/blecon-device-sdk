/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/reboot.h>

#include "stdio.h"
#include "string.h"
#include "stdlib.h"

#include "blecon/blecon.h"
#include "blecon/blecon_error.h"
#include "blecon/blecon_memfault_ota_client.h"
#include "blecon/blecon_download_client.h"
#include "blecon_zephyr/blecon_zephyr_event_loop.h"

#include "memfault/core/platform/device_info.h"

// OTA client callbacks
static void memfault_ota_client_on_ota_status_update(struct blecon_memfault_ota_client_t* client, char* ota_url, void* user_data);
static void memfault_ota_client_on_error(struct blecon_memfault_ota_client_t* client, void* user_data);

// Download client callbacks
static void download_client_on_new_chunk(struct blecon_download_client_t* client, uint8_t* data, size_t data_sz, bool done, void* user_data);
static void download_client_on_error(struct blecon_download_client_t* client, void* user_data);

// Events
static void ota_process_dfu_target_events(struct blecon_event_loop_t* event_loop, void* user_data);

// OTA thread
static void dfu_thread_fn(void*, void*, void*);

#define DFU_THREAD_STACK_SIZE 2048
#define DFU_THREAD_PRIORITY 5
#define MAX_CHUNKS_IN_FLIGHT 4

static struct blecon_event_loop_t* _event_loop = NULL;
static struct blecon_t* _blecon = {0};
static struct blecon_memfault_ota_parameters_t _memfault_ota_parameters = {0};
static struct blecon_memfault_ota_client_t _memfault_ota_client = {0};
static struct blecon_download_client_t _download_client = {0};
static bool _downloading_update = false;
static struct k_fifo _dfu_target_ops;
static struct k_fifo _dfu_target_events;
static uint32_t _process_completed_dfu_target_events_event_id = UINT32_MAX;

struct ota_dfu_target_op_t {
    bool reset;
    uint8_t* data;
    size_t data_sz;
    bool done;
};

enum ota_dfu_target_event_type_t {
    ota_dfu_target_event_type_chunk_written,
    ota_dfu_target_event_type_error
};
struct ota_dfu_target_event_t {
    enum ota_dfu_target_event_type_t event_type;
    uint8_t* chunk_data;
    bool done;
};

K_THREAD_STACK_DEFINE(_dfu_thread_stack, DFU_THREAD_STACK_SIZE);
struct k_thread _dfu_thread;

void ota_init(struct blecon_event_loop_t* event_loop, struct blecon_t* blecon) {
    _event_loop = event_loop;
    _blecon = blecon;

    sMemfaultDeviceInfo info = {0};
    memfault_platform_get_device_info(&info);
    _memfault_ota_parameters.hardware_version = info.hardware_version;
    _memfault_ota_parameters.software_type = info.software_type;
    _memfault_ota_parameters.software_version = info.software_version;

    const static struct blecon_memfault_ota_client_callbacks_t memfault_ota_client_callbacks = {
        .on_ota_status_update = memfault_ota_client_on_ota_status_update,
        .on_error = memfault_ota_client_on_error
    };
    blecon_memfault_ota_client_init(&_memfault_ota_client, blecon_get_request_processor(_blecon),
        &_memfault_ota_parameters, &memfault_ota_client_callbacks, NULL);

    // Check for update
    blecon_memfault_ota_client_check_for_update(&_memfault_ota_client);

    // Init FIFOs
    k_fifo_init(&_dfu_target_ops);
    k_fifo_init(&_dfu_target_events);

    // Register event
    _process_completed_dfu_target_events_event_id = blecon_zephyr_event_loop_assign_event(_event_loop, ota_process_dfu_target_events, NULL);

    // Start thread
    k_thread_create(&_dfu_thread, _dfu_thread_stack, K_THREAD_STACK_SIZEOF(_dfu_thread_stack),
                dfu_thread_fn, NULL, NULL, NULL,
                DFU_THREAD_PRIORITY, 0, K_NO_WAIT);
}

void ota_check_for_update(void) {
    if(!_downloading_update) {
        blecon_memfault_ota_client_check_for_update(&_memfault_ota_client);
    }
}

void memfault_ota_client_on_ota_status_update(struct blecon_memfault_ota_client_t* client, char* ota_url, void* user_data) {
    if(ota_url == NULL) {
        printk("Firmware is up to date.\r\n");
        if(!boot_is_img_confirmed()) {
            printk("Confirming firmware update\r\n");
            boot_write_img_confirmed();
        }
        return;
    }
    else if(_downloading_update) {
        printk("Update already downloading\r\n");
        free(ota_url);
        return;
    }
    
    printk("Downloading update from %s\r\n", ota_url);
    _downloading_update = true;

    const static struct blecon_download_client_callbacks_t download_client_callbacks = {
        .on_new_chunk = download_client_on_new_chunk,
        .on_error = download_client_on_error
    };
    blecon_download_client_init(&_download_client, blecon_get_request_processor(_blecon), MAX_CHUNKS_IN_FLIGHT, &download_client_callbacks, NULL);

    // Reset DFU target
    struct ota_dfu_target_op_t* reset_op = malloc(sizeof(struct ota_dfu_target_op_t));
    blecon_assert(reset_op != NULL);
    memset(reset_op, 0, sizeof(struct ota_dfu_target_op_t));
    reset_op->reset = true;
    k_fifo_put(&_dfu_target_ops, reset_op);

    // Download
    blecon_download_client_download(&_download_client, ota_url, 0, 1*1024*1024, NULL);
}

void memfault_ota_client_on_error(struct blecon_memfault_ota_client_t* client, void* user_data) {
    printk("Failed to get OTA URL\r\n");

    // Try again
    // blecon_memfault_ota_client_check_for_update(&_memfault_ota_client);
}

void download_client_on_new_chunk(struct blecon_download_client_t* client, uint8_t* data, size_t data_sz, bool done, void* user_data) {
    printk("Downloaded %zu bytes (chunk %p)\r\n", data_sz, data);
    if(done) {
        printk("Download complete\r\n");
    }

    struct ota_dfu_target_op_t* op = malloc(sizeof(struct ota_dfu_target_op_t));
    blecon_assert(op != NULL);
    op->reset = false;
    op->data = data;
    op->data_sz = data_sz;
    op->done = done;

    k_fifo_put(&_dfu_target_ops, op);
}

void download_client_on_error(struct blecon_download_client_t* client, void* user_data) {
    printk("Download failed\r\n");
    _downloading_update = false;
}

void ota_process_dfu_target_events(struct blecon_event_loop_t* event_loop, void* user_data) {
    do {
        struct ota_dfu_target_event_t* event = k_fifo_get(&_dfu_target_events, K_NO_WAIT);
        if(event == NULL) {
            return;
        }

        switch (event->event_type)
        {
        case ota_dfu_target_event_type_chunk_written:
            // Free chunk
            printk("Chunk %p written\r\n", event->chunk_data);
            blecon_download_client_free_chunk(&_download_client, event->chunk_data);
            if(event->done) {
                // Check if we're done
                if(!_download_client.downloading) {
                    // Done
                    _downloading_update = false;
                    printk("Update complete, rebooting\r\n");

                    boot_request_upgrade(BOOT_UPGRADE_TEST);
		            sys_reboot(SYS_REBOOT_WARM);
                }
            }
            break;
        case ota_dfu_target_event_type_error:
        default:
            printk("OTA failed\r\n");
            _downloading_update = false;
            // Process error event
            break;
        }

        free(event);
    } while(true);
}

static struct flash_img_context _flash_ctx = {0};
void dfu_thread_fn(void*, void*, void*) {
    while(true) {
        struct ota_dfu_target_op_t* op = k_fifo_get(&_dfu_target_ops, K_FOREVER);
        if(op->reset) {
            int ret = flash_img_init_id(&_flash_ctx, FIXED_PARTITION_ID(mcuboot_secondary));
            blecon_assert(ret == 0);
        } else {
            int ret = flash_img_buffered_write(&_flash_ctx, op->data, op->data_sz, op->done);
            blecon_assert(ret == 0);

            // Send events back
            struct ota_dfu_target_event_t* event = malloc(sizeof(struct ota_dfu_target_event_t));
            blecon_assert(event != NULL);
            event->event_type = ota_dfu_target_event_type_chunk_written;
            event->chunk_data = op->data;
            event->done = op->done;
            k_fifo_put(&_dfu_target_events, event);
        }
        free(op);

        // Signal event loop
        blecon_zephyr_event_loop_post_event(_event_loop, _process_completed_dfu_target_events_event_id);
    }
}
