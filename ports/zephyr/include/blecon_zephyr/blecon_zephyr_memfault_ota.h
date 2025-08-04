/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <zephyr/dfu/flash_img.h>

#include "blecon/blecon.h"

#include "blecon/blecon_memfault_ota_client.h"
#include "blecon/blecon_download_client.h"


struct blecon_zephyr_memfault_ota_t;

enum blecon_zephyr_memfault_ota_status_t {
    blecon_zephyr_memfault_ota_status_idle,                                            /**< Not downloading an update */
    blecon_zephyr_memfault_ota_status_downloading,                                     /**< Currently downloading an update */
    blecon_zephyr_memfault_ota_status_no_update_available,                             /**< No update available */
    blecon_zephyr_memfault_ota_status_error_download_failed,                           /**< Error: Download failed */
    blecon_zephyr_memfault_ota_status_error_bad_url,                                   /**< Error: Bad URL */
    blecon_zephyr_memfault_ota_status_error_during_dfu,                                /**< Error: Could not write to flash during DFU */
    blecon_zephyr_memfault_ota_status_error_installation_attempted_during_download,    /**< Error: Installation attempted during download */
};

/**
 * @brief Callbacks structure for OTA operations
 *
 * Contains function pointers for OTA event handling
 */
struct blecon_zephyr_memfault_ota_callbacks_t {
    /**
     * @brief Called when an OTA download is initiated
     * @param ota the OTA instance
     */
    void (*on_ota_start)(struct blecon_zephyr_memfault_ota_t* ota, const char *ota_url);

    /**
     * @brief Called when an OTA download has completed successfully
     * @param ota the OTA instance
     * @param bytes_downloaded the total number of bytes downloaded
     */
    void (*on_ota_complete)(struct blecon_zephyr_memfault_ota_t* ota, size_t bytes_downloaded);

    /**
     * @brief Called when an OTA error occurs
     * @param ota the OTA instance
     */
    void (*on_ota_error)(struct blecon_zephyr_memfault_ota_t* ota, enum blecon_zephyr_memfault_ota_status_t status);

    /**
     * @brief Called periodically to report OTA download progress
     * @param ota the OTA instance
     * @param bytes_downloaded the number of bytes downloaded so far
     */
    void (*on_ota_progress)(struct blecon_zephyr_memfault_ota_t* ota, size_t bytes_downloaded);
};

/**
 * @brief Structure representing an OTA update manager
 *
 * Manages the over-the-air firmware update process including checking for updates,
 * downloading firmware, and applying updates
 */
struct blecon_zephyr_memfault_ota_t {
    /** The event loop used for OTA operations */
    struct blecon_event_loop_t* event_loop;

    /** The Blecon instance used for communication */
    struct blecon_t* blecon;

    /** The namespace for OTA requests */
    const char* request_namespace;

    /** Callbacks for OTA operations */
    const struct blecon_zephyr_memfault_ota_callbacks_t *callbacks;

    /** User data to be passed to callbacks */
    void* user_data;

    /** Parameters for Memfault OTA client */
    struct blecon_memfault_ota_parameters_t memfault_ota_parameters;

    /** Memfault OTA client instance */
    struct blecon_memfault_ota_client_t memfault_ota_client;

    /** Download client for firmware retrieval */
    struct blecon_download_client_t download_client;

    /** Flag indicating if an update is currently downloading */
    bool downloading_update;

    /** FIFO queue for DFU target operations */
    struct k_fifo dfu_target_ops;

    /** FIFO queue for DFU target events */
    struct k_fifo dfu_target_events;

    /** Event for processing completed DFU target events */
    struct blecon_event_t* process_completed_dfu_target_events_event;

    /** Flash context for managing firmware image writing */
    struct flash_img_context flash_ctx;
};

/**
 * @brief Initialize an OTA instance
 *
 * Sets up the OTA manager with the given parameters and begins checking for updates
 *
 * @param ota the OTA instance to initialize
 * @param event_loop the event loop to use for OTA operations
 * @param blecon the Blecon instance to use for communication
 * @param request_namespace the namespace for OTA requests
 * @param callbacks a pointer to the callback functions for OTA operations
 * @param user_data user data to pass to the callbacks
 */
void blecon_zephyr_memfault_ota_init(struct blecon_zephyr_memfault_ota_t* ota, struct blecon_event_loop_t* event_loop, struct blecon_t* blecon, const char* request_namespace, const struct blecon_zephyr_memfault_ota_callbacks_t *callbacks, void* user_data);

/**
 * @brief Check for available OTA updates
 *
 * Initiates a check for firmware updates from the Memfault service
 *
 * @param ota the OTA instance
 */
void blecon_zephyr_memfault_ota_check_for_update(struct blecon_zephyr_memfault_ota_t* ota);

/**
 * @brief Install a downloaded update and reboot the device
 *
 * Applies a downloaded update and performs a system reboot to boot into the new firmware
 *
 * @param ota the OTA instance
 */
void blecon_zephyr_memfault_ota_install_and_reboot(struct blecon_zephyr_memfault_ota_t* ota);

/**
 * @brief Confirm the current firmware image
 *
 * This function marks the currently running firmware image as confirmed, which means it will be used on the next boot.
 *
 * Warning: this function *must* be called after a successful OTA update to ensure the new firmware is used on the next boot.
 *
 * If this function is not called, the device will revert to the previous firmware image on the next boot.
 *
 * @param ota the OTA instance
 */
void blecon_zephyr_memfault_ota_confirm_current_image(struct blecon_zephyr_memfault_ota_t* ota);

/**
 * @brief Get the user data associated with the OTA instance
 *
 * @param ota the OTA instance
 * @return a pointer to the user data
 */
void *blecon_zephyr_memfault_ota_get_user_data(struct blecon_zephyr_memfault_ota_t* ota);

/**
 * @brief Get the current OTA status
 *
 * @param ota the OTA instance
 * @return the current OTA status
 */
enum blecon_zephyr_memfault_ota_status_t blecon_zephyr_memfault_ota_get_status(struct blecon_zephyr_memfault_ota_t* ota);

/**
 * @brief Convert OTA status to string
 *
 * @param status the OTA status
 * @return a string representation of the OTA status
 */
const char *blecon_zephyr_memfault_ota_status_to_string(enum blecon_zephyr_memfault_ota_status_t status);

#ifdef __cplusplus
}
#endif
