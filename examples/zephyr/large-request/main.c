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

#include "blecon/blecon.h"
#include "blecon/blecon_error.h"

#if defined(CONFIG_BLECON_INTERNAL_MODEM)
#include "blecon_zephyr/blecon_zephyr_event_loop.h"
#include "blecon_zephyr/blecon_zephyr_bluetooth.h"
#include "blecon_zephyr/blecon_zephyr_crypto.h"
#include "blecon_zephyr/blecon_zephyr_nvm.h"
#include "blecon_zephyr/blecon_zephyr_nfc.h"
#elif defined(CONFIG_BLECON_EXTERNAL_MODEM)
#include "blecon_zephyr/blecon_zephyr_event_loop.h"
#include "blecon_zephyr/blecon_zephyr_ext_modem_uart_transport.h"
#endif

#define CONCURRENT_SEND_OPS_COUNT 2

struct example_send_op_t {
    struct blecon_request_send_data_op_t op;
    bool busy;
};

static struct blecon_event_loop_t* _event_loop = NULL;
static struct blecon_t _blecon = {0};
static struct blecon_request_t _request = {0};
static struct example_send_op_t _send_ops[CONCURRENT_SEND_OPS_COUNT] = {0};
static struct blecon_request_receive_data_op_t _receive_op = {0};
static uint8_t _outgoing_data_buffer[CONFIG_LARGE_REQUEST_EXAMPLE_BUFFER_SZ] = {0}; // Large buffer
static size_t _outgoing_data_buffer_pos = 0;
static uint8_t _incoming_data_buffer[64] = {0};

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

// Internal functions
static void example_send_data(void);

// Shell commands
static int cmd_blecon_connection_initiate(const struct shell* sh, size_t argc, char** argv);
static int cmd_blecon_announce(const struct shell* sh, size_t argc, char** argv);

// Shell command event handlers
static void cmd_blecon_connection_initiate_event(struct blecon_event_loop_t* event_loop, void* user_data);
static void cmd_blecon_announce_event(struct blecon_event_loop_t* event_loop, void* user_data);

// Event handler ids
static uint32_t _cmd_blecon_connection_initiate_event_id = UINT32_MAX;
static uint32_t _cmd_blecon_announce_event_id = UINT32_MAX;

void example_on_connection(struct blecon_t* blecon) {
    printk("Connected, sending request.\r\n");

    // Clean-up request
    blecon_request_cleanup(&_request);
    
    // Reset outgoing buffer position
    _outgoing_data_buffer_pos = 0;

    // Reset send operations
    for(size_t p = 0; p < CONCURRENT_SEND_OPS_COUNT; p++) {
        _send_ops[p].busy = false;
    }
    
    // Queue initial send operations
    example_send_data();

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

    if(finished) {
        printk("All received\r\n");
    }
}

void example_request_on_data_sent(struct blecon_request_send_data_op_t* send_data_op, bool data_sent) {
    // Mark send operation as not busy
    struct example_send_op_t* op = (struct example_send_op_t*)blecon_request_send_data_op_get_user_data(send_data_op);
    op->busy = false;

    if(!data_sent) {
        printk("Failed to send data\r\n");
        return;
    }
    printk("Data sent\r\n");

    // Send more chunks if needed
    example_send_data();
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

void example_send_data(void) {
    for(size_t p = 0; p < CONCURRENT_SEND_OPS_COUNT; p++) {
        struct example_send_op_t* op = &_send_ops[p];
        if(op->busy) {
            continue;
        }

        if(_outgoing_data_buffer_pos >= sizeof(_outgoing_data_buffer)) {
            // Already finished
            break;
        }

        size_t chunk_sz = 4096;
        if(_outgoing_data_buffer_pos + chunk_sz > sizeof(_outgoing_data_buffer)) {
            chunk_sz = sizeof(_outgoing_data_buffer) - _outgoing_data_buffer_pos;
        }

        bool finished = false;
        if(_outgoing_data_buffer_pos + chunk_sz >= sizeof(_outgoing_data_buffer)) {
            finished = true;
        }

        printk("Sending chunk of %u bytes\r\n", chunk_sz);

        // Create send data operation
        if(!blecon_request_send_data(&op->op, &_request, _outgoing_data_buffer + _outgoing_data_buffer_pos, chunk_sz, finished, op)) {
            printk("Failed to send data\r\n");
            blecon_request_cleanup(&_request);
            return;
        }

        _outgoing_data_buffer_pos += chunk_sz;

        if(finished) {
            printk("All sent\r\n");
            break;
        }
    }
}

int main(void)
{
#if defined(CONFIG_USB_CDC_ACM)
    // Give a chance to UART to connect
    k_sleep(K_MSEC(1000));
#endif
    
#if defined(CONFIG_BLECON_INTERNAL_MODEM)
    // Init Event Loop
    _event_loop = blecon_zephyr_event_loop_new();

    // Init Bluetooth port
    struct blecon_bluetooth_t* bluetooth = blecon_zephyr_bluetooth_init(_event_loop);

    // Init Crypto port
    struct blecon_crypto_t* crypto = blecon_zephyr_crypto_init();

    // Init NVM port
    struct blecon_nvm_t* nvm = blecon_zephyr_nvm_init();

    // Init NFC port
    struct blecon_nfc_t* nfc = blecon_zephyr_nfc_init();

    // Init internal modem
    struct blecon_modem_t* modem = blecon_int_modem_create(
        _event_loop,
        bluetooth,
        crypto,
        nvm,
        nfc,
        malloc
    );
#elif defined(CONFIG_BLECON_EXTERNAL_MODEM)
    // Init Event Loop
    _event_loop = blecon_zephyr_event_loop_new();

    // Init UART device
    const struct device* uart_device = DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(blecon_modem)));

    // Init external modem transport
    struct blecon_zephyr_ext_modem_uart_transport_t ext_modem_uart_transport;
    blecon_zephyr_ext_modem_uart_transport_init(&ext_modem_uart_transport, _event_loop, uart_device);

    // Init external modem
    struct blecon_modem_t* modem = blecon_ext_modem_create(
        _event_loop,
        blecon_zephyr_ext_modem_uart_transport_as_transport(&ext_modem_uart_transport),
        malloc
    );
#else
    #error "No modem implementation selected - please enable a Blecon modem implementation in prj.conf"
#endif
    blecon_assert(modem != NULL); // Make sure allocation was successful

    // Register event ids for shell commands
    _cmd_blecon_connection_initiate_event_id = blecon_zephyr_event_loop_assign_event(_event_loop, cmd_blecon_connection_initiate_event, NULL);
    _cmd_blecon_announce_event_id = blecon_zephyr_event_loop_assign_event(_event_loop, cmd_blecon_announce_event, NULL);

    // Blecon
    blecon_init(&_blecon, modem);
    blecon_set_callbacks(&_blecon, &blecon_callbacks, NULL);
    if(!blecon_setup(&_blecon)) {
        printk("Failed to setup blecon\r\n");
        return 1;
    }

    // Init request
    const static struct blecon_request_parameters_t request_params = {
        .namespace = "mynamespace",
        .method = "upload",
        .oneway = true,
        .request_content_type = NULL,
        .response_content_type = NULL,
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

int cmd_blecon_connection_initiate(const struct shell* sh, size_t argc, char** argv) {
    if(_cmd_blecon_connection_initiate_event_id == UINT32_MAX) {
        shell_print(sh, "Event id not assigned");
        return -1;
    }
    blecon_zephyr_event_loop_post_event(_event_loop, _cmd_blecon_connection_initiate_event_id);
    return 0;
}

int cmd_blecon_announce(const struct shell* sh, size_t argc, char** argv) {
    if(_cmd_blecon_announce_event_id == UINT32_MAX) {
        shell_print(sh, "Event id not assigned");
        return -1;
    }
    blecon_zephyr_event_loop_post_event(_event_loop, _cmd_blecon_announce_event_id);
    return 0;
}

void cmd_blecon_connection_initiate_event(struct blecon_event_loop_t* event_loop, void* user_data) {
    // Initiate connection
    if(!blecon_connection_initiate(&_blecon)) {
        printk("Failed to initiate connection\r\n");
        return;
    }
}

void cmd_blecon_announce_event(struct blecon_event_loop_t* event_loop, void* user_data) {
    // Announce device ID
    if(!blecon_announce(&_blecon)) {
        printk("Failed to announce device ID\r\n");
        return;
    }
}


SHELL_STATIC_SUBCMD_SET_CREATE(blecon,
	SHELL_CMD(connection_initiate, NULL, "Initiate connection to Blecon network.", cmd_blecon_connection_initiate),
    SHELL_CMD(announce, NULL, "Announce Blecon device ID to nearby hotspots.", cmd_blecon_announce),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(blecon, &blecon, "Blecon commands", NULL);
