/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>

#include "stdio.h"
#include "string.h"

#include "blecon/blecon.h"
#include "blecon/blecon_error.h"
#include "blecon_zephyr/blecon_zephyr.h"
#include "blecon_zephyr/blecon_zephyr_event_loop.h"

static struct blecon_event_loop_t* _event_loop = NULL;
static struct blecon_t _blecon = {0};
static struct blecon_request_t _request = {0};
static struct blecon_request_send_data_op_t _send_op = {0};
static struct blecon_request_receive_data_op_t _receive_op = {0};
static uint8_t _outgoing_data_buffer[64] = {0};
static uint8_t _incoming_data_buffer[64] = {0};

static enum blecon_advertising_mode_t _current_mode = blecon_advertising_mode_balanced;

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

// Requests callbacks
static void example_request_on_closed(struct blecon_request_t* request);
static void example_request_on_data_sent(struct blecon_request_send_data_op_t* send_data_op, bool data_sent);
static uint8_t* example_request_alloc_incoming_data_buffer(struct blecon_request_receive_data_op_t* receive_data_op, size_t sz);
static void example_request_on_data_received(struct blecon_request_receive_data_op_t* receive_data_op, bool data_received, const uint8_t* data, size_t sz, bool finished);

const static struct blecon_request_callbacks_t blecon_request_callbacks = {
    .on_closed = example_request_on_closed,
    .on_data_sent = example_request_on_data_sent,
    .alloc_incoming_data_buffer = example_request_alloc_incoming_data_buffer,
    .on_data_received = example_request_on_data_received
};

// Shell commands
static int cmd_blecon_connection_initiate(const struct shell* sh, size_t argc, char** argv);
static int cmd_blecon_set_mode(const struct shell* sh, size_t argc, char** argv);
static int cmd_blecon_get_mode(const struct shell* sh, size_t argc, char** argv);

// Shell command event handlers
static void cmd_blecon_connection_initiate_event(struct blecon_event_t* event, void* user_data);

// Event handlers
static struct blecon_event_t* _cmd_blecon_connection_initiate_event = NULL;

static const char* mode_to_string(enum blecon_advertising_mode_t mode) {
    switch(mode) {
        case blecon_advertising_mode_high_performance:
            return "high_performance";
        case blecon_advertising_mode_balanced:
            return "balanced";
        case blecon_advertising_mode_ultra_low_power:
            return "ultra_low_power";
        default:
            return "unknown";
    }
}

static void print_mode_description(enum blecon_advertising_mode_t mode) {
    switch(mode) {
        case blecon_advertising_mode_high_performance:
            printk("  Mode: High Performance\r\n");
            printk("  - 100ms advertising period\r\n");
            printk("  - Highest power consumption\r\n");
            break;
        case blecon_advertising_mode_balanced:
            printk("  Mode: Balanced\r\n");
            printk("  - 1s default advertising period, alternates between 100ms and 1s when requesting connection\r\n");
            printk("  - Moderate power consumption\r\n");
            break;
        case blecon_advertising_mode_ultra_low_power:
            printk("  Mode: Ultra Low Power\r\n");
            printk("  - Only advertises when requesting connection\r\n");
            printk("  - Lowest power consumption\r\n");
            break;
        default:
            printk("  Unknown mode\r\n");
            break;
    }
}

void example_on_connection(struct blecon_t* blecon) {
    printk("Connected, sending request.\r\n");

    // Clean-up request
    blecon_request_cleanup(&_request);
    
    // Create message with current mode
    sprintf((char*)_outgoing_data_buffer, "Hello from %s mode!", mode_to_string(_current_mode));

    // Create send data operation
    if(!blecon_request_send_data(&_send_op, &_request, _outgoing_data_buffer,
        strlen((char*)_outgoing_data_buffer), true, NULL)) {
        printk("Failed to send data\r\n");
        blecon_request_cleanup(&_request);
        return;
    }

    // Create receive data operation
    if(!blecon_request_receive_data(&_receive_op, &_request, NULL)) {
        printk("Failed to receive data\r\n");
        blecon_request_cleanup(&_request);
        return;
    }

    // Submit request
    blecon_submit_request(&_blecon, &_request);
}

void example_on_disconnection(struct blecon_t* blecon) {
    printk("Disconnected\r\n");
}

void example_on_time_update(struct blecon_t* blecon) {
    printk("Time update\r\n");
}

void example_on_ping_result(struct blecon_t* blecon) {}

void example_request_on_data_received(struct blecon_request_receive_data_op_t* receive_data_op, bool data_received, const uint8_t* data, size_t sz, bool finished) {
    // Retrieve response
    if(!data_received) {
        printk("Failed to receive data\r\n");
        return;
    }
    printk("Data received\r\n");

    static char message[64] = {0};
    memset(message, 0, sizeof(message));
    memcpy(message, data, sz);

    printk("Response: %s\r\n", message);

    if(finished) {
        printk("All received\r\n");
    }
}

void example_request_on_data_sent(struct blecon_request_send_data_op_t* send_data_op, bool data_sent) {
    if(!data_sent) {
        printk("Failed to send data\r\n");
        return;
    }
    printk("Data sent\r\n");
}

uint8_t* example_request_alloc_incoming_data_buffer(struct blecon_request_receive_data_op_t* receive_data_op, size_t sz) {
    return _incoming_data_buffer;
}

void example_request_on_closed(struct blecon_request_t* request) {
    enum blecon_request_status_code_t status_code = blecon_request_get_status(request);

    if(status_code != blecon_request_status_ok) {
        printk("Request failed with status code: %d\r\n", status_code);
    } else {
        printk("Request successful\r\n");
    }

    // Terminate connection
    blecon_connection_terminate(&_blecon);
}

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
    _cmd_blecon_connection_initiate_event = blecon_event_loop_register_event(_event_loop, cmd_blecon_connection_initiate_event, NULL);

    // Blecon
    blecon_init(&_blecon, modem);
    blecon_set_callbacks(&_blecon, &blecon_callbacks, NULL);
    if(!blecon_setup(&_blecon)) {
        printk("Failed to setup blecon\r\n");
        return 1;
    }

    // Set initial advertising mode
    if(!blecon_set_advertising_mode(&_blecon, _current_mode)) {
        printk("Failed to set advertising mode\r\n");
        return 1;
    }

    // Init request
    const static struct blecon_request_parameters_t request_params = {
        .namespace = "global:blecon_util",
        .method = "echo",
        .oneway = false,
        .request_content_type = "text/plain",
        .response_content_type = "text/plain",
        .response_mtu = sizeof(_incoming_data_buffer),
        .callbacks = &blecon_request_callbacks,
        .user_data = NULL
    };
    blecon_request_init(&_request, &request_params);

    // Print device URL
    char blecon_url[BLECON_URL_SZ] = {0};
    if(!blecon_get_url(&_blecon, blecon_url, sizeof(blecon_url))) {
        printk("Failed to get device URL\r\n");
        return 1;
    }
    printk("Device URL: %s\r\n", blecon_url);

    // Print initial advertising mode
    printk("\r\nCurrent advertising mode: %s\r\n", mode_to_string(_current_mode));
    print_mode_description(_current_mode);
    printk("\r\n");

    // Enter main loop.
    blecon_event_loop_run(_event_loop);

    // Won't reach here
    return 0;
}

int cmd_blecon_connection_initiate(const struct shell* sh, size_t argc, char** argv) {
    if(_cmd_blecon_connection_initiate_event == NULL) {
        shell_print(sh, "Event not assigned");
        return -1;
    }
    blecon_event_signal(_cmd_blecon_connection_initiate_event);
    return 0;
}

int cmd_blecon_set_mode(const struct shell* sh, size_t argc, char** argv) {
    if(argc < 2) {
        shell_print(sh, "Usage: blecon set_mode <high_performance|balanced|ultra_low_power>");
        return -1;
    }

    enum blecon_advertising_mode_t new_mode;
    
    if(strcmp(argv[1], "high_performance") == 0) {
        new_mode = blecon_advertising_mode_high_performance;
    } else if(strcmp(argv[1], "balanced") == 0) {
        new_mode = blecon_advertising_mode_balanced;
    } else if(strcmp(argv[1], "ultra_low_power") == 0) {
        new_mode = blecon_advertising_mode_ultra_low_power;
    } else {
        shell_print(sh, "Invalid mode. Use: high_performance, balanced, or ultra_low_power");
        return -1;
    }

    if(!blecon_set_advertising_mode(&_blecon, new_mode)) {
        shell_print(sh, "Failed to set advertising mode");
        return -1;
    }

    _current_mode = new_mode;
    
    shell_print(sh, "\r\nAdvertising mode changed to: %s", mode_to_string(new_mode));
    print_mode_description(new_mode);
    shell_print(sh, "");
    
    return 0;
}

int cmd_blecon_get_mode(const struct shell* sh, size_t argc, char** argv) {
    shell_print(sh, "\r\nCurrent advertising mode: %s", mode_to_string(_current_mode));
    print_mode_description(_current_mode);
    shell_print(sh, "");
    return 0;
}

void cmd_blecon_connection_initiate_event(struct blecon_event_t* event, void* user_data) {
    // Initiate connection
    if(!blecon_connection_initiate(&_blecon)) {
        printk("Failed to initiate connection\r\n");
        return;
    }
}

SHELL_STATIC_SUBCMD_SET_CREATE(blecon,
    SHELL_CMD(connection_initiate, NULL, "Initiate connection to Blecon network.", cmd_blecon_connection_initiate),
    SHELL_CMD(set_mode, NULL, "Set advertising mode (high_performance|balanced|ultra_low_power).", cmd_blecon_set_mode),
    SHELL_CMD(get_mode, NULL, "Get current advertising mode.", cmd_blecon_get_mode),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(blecon, &blecon, "Blecon commands", NULL);
