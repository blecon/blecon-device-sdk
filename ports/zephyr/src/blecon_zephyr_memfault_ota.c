/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
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

#include "blecon_zephyr_memfault_ota.h"

// OTA client callbacks
static void memfault_ota_client_on_ota_status_update(struct blecon_memfault_ota_client_t* client, char* ota_url, void* user_data);
static void memfault_ota_client_on_error(struct blecon_memfault_ota_client_t* client, void* user_data);

// Download client callbacks
static void download_client_on_new_chunk(
    struct blecon_download_client_t* client, uint8_t* data, size_t data_sz, bool done, void* user_data);
static void download_client_on_error(struct blecon_download_client_t* client, void* user_data);

// Events
static void blecon_zephyr_memfault_ota_process_dfu_target_events(struct blecon_event_t* event, void* user_data);

// OTA thread
static void dfu_thread_fn(void*, void*, void*);

#define DFU_THREAD_STACK_SIZE 2048
#define DFU_THREAD_PRIORITY 5
#define MAX_CHUNKS_IN_FLIGHT 4


struct blecon_zephyr_memfault_ota_dfu_target_op_t {
    bool reset;
    uint8_t* data;
    size_t data_sz;
    bool done;
};

enum blecon_zephyr_memfault_ota_dfu_target_event_type_t {
    blecon_zephyr_memfault_ota_dfu_target_event_type_chunk_written,
    blecon_zephyr_memfault_ota_dfu_target_event_type_error
};
struct blecon_zephyr_memfault_ota_dfu_target_event_t {
    enum blecon_zephyr_memfault_ota_dfu_target_event_type_t event_type;
    uint8_t* chunk_data;
    bool done;
};

K_THREAD_STACK_DEFINE(_dfu_thread_stack, DFU_THREAD_STACK_SIZE);
struct k_thread _dfu_thread;

static struct blecon_zephyr_memfault_ota_t* current_ota = NULL;

void blecon_zephyr_memfault_ota_init(struct blecon_zephyr_memfault_ota_t* ota, struct blecon_event_loop_t* event_loop, struct blecon_t* blecon, const char* request_namespace, const struct blecon_zephyr_memfault_ota_callbacks_t *callbacks, void* user_data) {

    ota->event_loop = event_loop;
    ota->blecon = blecon;
    ota->request_namespace = request_namespace;
    ota->callbacks = callbacks;
    ota->user_data = user_data;


    sMemfaultDeviceInfo info = {0};
    memfault_platform_get_device_info(&info);
    ota->memfault_ota_parameters.hardware_version = info.hardware_version;
    ota->memfault_ota_parameters.software_type = info.software_type;
    ota->memfault_ota_parameters.software_version = info.software_version;

    const static struct blecon_memfault_ota_client_callbacks_t memfault_ota_client_callbacks = {
        .on_ota_status_update = memfault_ota_client_on_ota_status_update,
        .on_error = memfault_ota_client_on_error
    };
    blecon_memfault_ota_client_init(&ota->memfault_ota_client, blecon_get_request_processor(ota->blecon),
        &ota->memfault_ota_parameters, request_namespace, &memfault_ota_client_callbacks, ota);

    // Check for update
    blecon_memfault_ota_client_check_for_update(&ota->memfault_ota_client);

    // Init FIFOs
    k_fifo_init(&ota->dfu_target_ops);
    k_fifo_init(&ota->dfu_target_events);

    // Register event
    ota->process_completed_dfu_target_events_event = blecon_event_loop_register_event(ota->event_loop, blecon_zephyr_memfault_ota_process_dfu_target_events, ota);

    current_ota = ota;
    // Start thread
    k_thread_create(&_dfu_thread, _dfu_thread_stack, K_THREAD_STACK_SIZEOF(_dfu_thread_stack),
                dfu_thread_fn, NULL, NULL, NULL,
                DFU_THREAD_PRIORITY, 0, K_NO_WAIT);
}

void blecon_zephyr_memfault_ota_check_for_update(struct blecon_zephyr_memfault_ota_t* ota) {
    if(!ota->downloading_update) {
        blecon_memfault_ota_client_check_for_update(&ota->memfault_ota_client);
    }
}

void blecon_zephyr_memfault_ota_confirm_current_image(struct blecon_zephyr_memfault_ota_t* ota) {
    if(boot_is_img_confirmed()) {
        return;
    }
    boot_write_img_confirmed();
}

void memfault_ota_client_on_ota_status_update(struct blecon_memfault_ota_client_t* client, char* ota_url, void* user_data) {
    struct blecon_zephyr_memfault_ota_t* ota = (struct blecon_zephyr_memfault_ota_t*)user_data;
    if(ota_url == NULL) {
        if (ota->callbacks->on_ota_error) {
            ota->callbacks->on_ota_error(ota, blecon_zephyr_memfault_ota_status_no_update_available);
        }
        return;
    }
    else if(ota->downloading_update) {
        free(ota_url);
        return;
    }

    ota->downloading_update = true;

    if (ota->callbacks->on_ota_start) {
        ota->callbacks->on_ota_start(ota, ota_url);
    }

    const static struct blecon_download_client_callbacks_t download_client_callbacks = {
        .on_new_chunk = download_client_on_new_chunk,
        .on_error = download_client_on_error
    };
    blecon_download_client_init(&ota->download_client, blecon_get_request_processor(ota->blecon), MAX_CHUNKS_IN_FLIGHT, &download_client_callbacks, ota);

    // Reset DFU target
    struct blecon_zephyr_memfault_ota_dfu_target_op_t* reset_op = malloc(sizeof(struct blecon_zephyr_memfault_ota_dfu_target_op_t));
    blecon_assert(reset_op != NULL);
    memset(reset_op, 0, sizeof(struct blecon_zephyr_memfault_ota_dfu_target_op_t));
    reset_op->reset = true;
    k_fifo_put(&ota->dfu_target_ops, reset_op);

    // Download
    blecon_download_client_download(&ota->download_client, ota_url, 0, 1*1024*1024, ota);
}

void memfault_ota_client_on_error(struct blecon_memfault_ota_client_t* client, void* user_data) {
    struct blecon_zephyr_memfault_ota_t* ota = (struct blecon_zephyr_memfault_ota_t*)user_data;
    if (ota->callbacks->on_ota_error) {
        ota->callbacks->on_ota_error(ota, blecon_zephyr_memfault_ota_status_error_bad_url);
    }
}

void download_client_on_new_chunk(struct blecon_download_client_t* client, uint8_t* data, size_t data_sz, bool done, void* user_data) {
    struct blecon_zephyr_memfault_ota_t* ota = (struct blecon_zephyr_memfault_ota_t*)user_data;

    struct blecon_zephyr_memfault_ota_dfu_target_op_t* op = malloc(sizeof(struct blecon_zephyr_memfault_ota_dfu_target_op_t));
    blecon_assert(op != NULL);
    op->reset = false;
    op->data = data;
    op->data_sz = data_sz;
    op->done = done;

    k_fifo_put(&ota->dfu_target_ops, op);
}

void download_client_on_error(struct blecon_download_client_t* client, void* user_data) {
    struct blecon_zephyr_memfault_ota_t* ota = (struct blecon_zephyr_memfault_ota_t*)user_data;
    ota->downloading_update = false;

    if (ota->callbacks->on_ota_error) {
        ota->callbacks->on_ota_error(ota, blecon_zephyr_memfault_ota_status_error_download_failed);
    }
}

void blecon_zephyr_memfault_ota_process_dfu_target_events(struct blecon_event_t* event, void* user_data) {
    struct blecon_zephyr_memfault_ota_t* ota = (struct blecon_zephyr_memfault_ota_t*)user_data;
    do {
        struct blecon_zephyr_memfault_ota_dfu_target_event_t* event = k_fifo_get(&ota->dfu_target_events, K_NO_WAIT);
        if(event == NULL) {
            return;
        }

        switch (event->event_type)
        {
        case blecon_zephyr_memfault_ota_dfu_target_event_type_chunk_written:
            // Free chunk
            blecon_download_client_free_chunk(&ota->download_client, event->chunk_data);
            if (ota->callbacks->on_ota_progress) {
                ota->callbacks->on_ota_progress(ota, ota->flash_ctx.stream.bytes_written);
            }
            if(event->done) {
                // Check if we're done
                if(!ota->download_client.downloading) {
                    // Done
                    ota->downloading_update = false;
                    if (ota->callbacks->on_ota_complete) {
                        ota->callbacks->on_ota_complete(ota, ota->flash_ctx.stream.bytes_written);
                    }
                }
            }
            break;
        case blecon_zephyr_memfault_ota_dfu_target_event_type_error:
        default:
            ota->downloading_update = false;
            // Process error event

            if (ota->callbacks->on_ota_error) {
                ota->callbacks->on_ota_error(ota, blecon_zephyr_memfault_ota_status_error_during_dfu);
            }
            break;
        }

        free(event);
    } while(true);
}

void dfu_thread_fn(void*, void*, void*) {
    struct blecon_zephyr_memfault_ota_t* ota = current_ota;
    while(true) {
        struct blecon_zephyr_memfault_ota_dfu_target_op_t* op = k_fifo_get(&ota->dfu_target_ops, K_FOREVER);
        if(op->reset) {
            int ret = flash_img_init_id(&ota->flash_ctx, FIXED_PARTITION_ID(mcuboot_secondary));
            blecon_assert(ret == 0);
        } else {
            int ret = flash_img_buffered_write(&ota->flash_ctx, op->data, op->data_sz, op->done);
            blecon_assert(ret == 0);

            // Send events back
            struct blecon_zephyr_memfault_ota_dfu_target_event_t* event = malloc(sizeof(struct blecon_zephyr_memfault_ota_dfu_target_event_t));
            blecon_assert(event != NULL);
            event->event_type = blecon_zephyr_memfault_ota_dfu_target_event_type_chunk_written;
            event->chunk_data = op->data;
            event->done = op->done;
            k_fifo_put(&ota->dfu_target_events, event);
        }
        free(op);

        // Signal event loop
        blecon_event_on_raised(current_ota->process_completed_dfu_target_events_event);
    }
}

void blecon_zephyr_memfault_ota_install_and_reboot(struct blecon_zephyr_memfault_ota_t* ota) {
    // Install and reboot
    if(!ota->downloading_update) {
        boot_request_upgrade(BOOT_UPGRADE_TEST);
        sys_reboot(SYS_REBOOT_WARM);
    } else {
        if (ota->callbacks->on_ota_error) {
            ota->callbacks->on_ota_error(ota, blecon_zephyr_memfault_ota_status_error_installation_attempted_during_download);
        }
    }
}

void *blecon_zephyr_memfault_ota_get_user_data(struct blecon_zephyr_memfault_ota_t* ota) {
    return ota->user_data;
}

enum blecon_zephyr_memfault_ota_status_t blecon_zephyr_memfault_ota_get_status(struct blecon_zephyr_memfault_ota_t* ota) {
    if(ota->downloading_update) {
        return blecon_zephyr_memfault_ota_status_downloading;
    }
    return blecon_zephyr_memfault_ota_status_idle;
}

const char *blecon_zephyr_memfault_ota_status_to_string(enum blecon_zephyr_memfault_ota_status_t status) {
    switch (status) {
        case blecon_zephyr_memfault_ota_status_idle:
            return "idle";
        case blecon_zephyr_memfault_ota_status_downloading:
            return "downloading update";
        case blecon_zephyr_memfault_ota_status_no_update_available:
            return "no update available";
        case blecon_zephyr_memfault_ota_status_error_download_failed:
            return "error: download failed";
        case blecon_zephyr_memfault_ota_status_error_bad_url:
            return "error: bad url";
        case blecon_zephyr_memfault_ota_status_error_during_dfu:
            return "error during dfu flash operation";
        case blecon_zephyr_memfault_ota_status_error_installation_attempted_during_download:
            return "error: installation attempted during download";
        default:
            return "unknown status";
    }
}