/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>

#include "stdio.h"
#include "string.h"
#include "stdlib.h"

#include "ota.h"

#include "blecon/blecon.h"
#include "blecon/blecon_error.h"
#include "blecon/blecon_memfault_client.h"
#include "blecon/blecon_memfault_ota_client.h"
#include "blecon/blecon_download_client.h"
#include "blecon_zephyr/blecon_zephyr.h"
#include "blecon_zephyr/blecon_zephyr_event_loop.h"
#include "blecon_zephyr/blecon_zephyr_memfault.h"

#include "memfault/components.h"

#define MEMFAULT_REQUEST_NAMESPACE "memfault"

static struct blecon_event_loop_t* _event_loop = NULL;
static struct blecon_t _blecon = {0};
static struct blecon_memfault_client_t _memfault_client = {0};

// Blecon callbacks
static void example_on_connection(struct blecon_t* blecon);
static void example_on_disconnection(struct blecon_t* blecon);
static void example_on_time_update(struct blecon_t* blecon);
static void example_on_ping_result(struct blecon_t* blecon);

const static struct blecon_callbacks_t blecon_callbacks = {
    .on_connection = example_on_connection,
    .on_disconnection = example_on_disconnection,
    .on_time_update = example_on_time_update,
    .on_ping_result = example_on_ping_result
};

// Shell commands
static int cmd_blecon_memfault_sync(const struct shell* sh, size_t argc, char** argv);
static int cmd_blecon_announce(const struct shell* sh, size_t argc, char** argv);
static int cmd_crash(const struct shell* sh, size_t argc, char** argv);

// Shell command event handlers
static void cmd_blecon_memfault_sync_event(struct blecon_event_t* event, void* user_data);
static void cmd_blecon_announce_event(struct blecon_event_t* event, void* user_data);

// Event handler ids
static struct blecon_event_t* _cmd_blecon_memfault_sync_event = NULL;
static struct blecon_event_t* _cmd_blecon_announce_event = NULL;

void example_on_connection(struct blecon_t* blecon) {
    printk("Connected\r\n");
}

void example_on_disconnection(struct blecon_t* blecon) {
    printk("Disconnected\r\n");
}

void example_on_time_update(struct blecon_t* blecon) {
    printk("Time update\r\n");
}

void example_on_ping_result(struct blecon_t* blecon) {}

int main(void)
{
#if defined(CONFIG_USB_CDC_ACM)
    // Give a chance to UART to connect
    k_sleep(K_MSEC(1000));
#endif

    // Get event loop
    _event_loop = blecon_zephyr_get_event_loop();

    // Get modem
    struct blecon_modem_t* modem = blecon_zephyr_get_modem();

    // Register event ids for shell commands
    _cmd_blecon_memfault_sync_event = blecon_event_loop_register_event(_event_loop, cmd_blecon_memfault_sync_event, NULL);
    _cmd_blecon_announce_event = blecon_event_loop_register_event(_event_loop, cmd_blecon_announce_event, NULL);

    // Memfault
    struct blecon_memfault_t* memfault = blecon_zephyr_memfault_init();

    // Blecon
    blecon_init(&_blecon, modem);
    blecon_set_callbacks(&_blecon, &blecon_callbacks, NULL);
    if(!blecon_setup(&_blecon)) {
        printk("Failed to setup blecon\r\n");
        return 1;
    }

    uint8_t blecon_id[BLECON_UUID_SZ] = {0};
    if(!blecon_get_identity(&_blecon, blecon_id)) {
        printk("Failed to get identity\r\n");
        return 1;
    }
    blecon_memfault_client_init(&_memfault_client, blecon_get_request_processor(&_blecon), memfault, blecon_id, MEMFAULT_REQUEST_NAMESPACE);

    // Print firmware version
    sMemfaultDeviceInfo mftlt_device_info = {0};
    memfault_platform_get_device_info(&mftlt_device_info);
    printk("Hardware version: %s\r\n", mftlt_device_info.hardware_version);
    printk("Software version: %s\r\n", mftlt_device_info.software_version);

    // Print device URL
    char blecon_url[BLECON_URL_SZ] = {0};
    if(!blecon_get_url(&_blecon, blecon_url, sizeof(blecon_url))) {
        printk("Failed to get device URL\r\n");
        return 1;
    }
    printk("Device URL: %s\r\n", blecon_url);

    // Init OTA module
    ota_init(_event_loop, &_blecon, MEMFAULT_REQUEST_NAMESPACE);

    // Initiate connection
    if(!blecon_connection_initiate(&_blecon)) {
        printk("Failed to initiate connection\r\n");
        return 1;
    }

    // Enter main loop.
    blecon_event_loop_run(_event_loop);

    // Won't reach here
    return 0;
}

int cmd_blecon_memfault_sync(const struct shell* sh, size_t argc, char** argv) {
    if(_cmd_blecon_memfault_sync_event == NULL) {
        shell_print(sh, "Event id not assigned");
        return -1;
    }
    blecon_event_signal(_cmd_blecon_memfault_sync_event);
    return 0;
}

int cmd_blecon_announce(const struct shell* sh, size_t argc, char** argv) {
    if(_cmd_blecon_announce_event == NULL) {
        shell_print(sh, "Event id not assigned");
        return -1;
    }
    blecon_event_signal(_cmd_blecon_announce_event);
    return 0;
}

int cmd_crash(const struct shell* sh, size_t argc, char** argv) {
    // Crash the device
    size_t* crash = NULL;
    *crash = 0;
    return 0;
}

void cmd_blecon_memfault_sync_event(struct blecon_event_t* event, void* user_data) {
    // Initiate connection
    if(!blecon_connection_initiate(&_blecon)) {
        printk("Failed to initiate connection\r\n");
        return;
    }

    // Check for update
    ota_check_for_update();
}

void cmd_blecon_announce_event(struct blecon_event_t* event, void* user_data) {
    // Announce device ID
    if(!blecon_announce(&_blecon)) {
        printk("Failed to announce device ID\r\n");
        return;
    }
}

SHELL_STATIC_SUBCMD_SET_CREATE(blecon,
	SHELL_CMD(memfault_sync, NULL, "Initiate connection to Blecon network and sync memfault chunks and OTA.", cmd_blecon_memfault_sync),
    SHELL_CMD(announce, NULL, "Announce Blecon device ID to nearby hotspots.", cmd_blecon_announce),
    SHELL_CMD(crash, NULL, "Crash the device", cmd_crash),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(blecon, &blecon, "Blecon commands", NULL);
