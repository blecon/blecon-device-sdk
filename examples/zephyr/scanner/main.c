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
#include "blecon/blecon_util.h"
#include "blecon_zephyr/blecon_zephyr.h"
#include "blecon_zephyr/blecon_zephyr_event_loop.h"

static struct blecon_event_loop_t* _event_loop = NULL;
static struct blecon_t _blecon = {0};
static struct blecon_request_t _request = {0};
static struct blecon_request_send_data_op_t _send_op = {0};
static struct blecon_request_receive_data_op_t _receive_op = {0};
static uint8_t _outgoing_data_buffer[64] = {0};
static uint8_t _incoming_data_buffer[64] = {0};

// Blecon callbacks
static void example_on_connection(struct blecon_t* blecon);
static void example_on_disconnection(struct blecon_t* blecon);
static void example_on_time_update(struct blecon_t* blecon);
static void example_on_ping_result(struct blecon_t* blecon);
static void example_on_scan_report(struct blecon_t* blecon);
static void example_on_scan_complete(struct blecon_t* blecon);

const static struct blecon_callbacks_t blecon_callbacks = {
    .on_connection = example_on_connection,
    .on_disconnection = example_on_disconnection,
    .on_time_update = example_on_time_update,
    .on_ping_result = example_on_ping_result,
    .on_scan_report = example_on_scan_report,
    .on_scan_complete = example_on_scan_complete,
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

// Scan report iterators
static void example_raw_scan_report_iterator(const struct blecon_modem_raw_scan_report_t* report, void* user_data);
static void example_peer_scan_report_iterator(const struct blecon_modem_peer_scan_report_t* report, void* user_data);

// Shell commands
static int cmd_blecon_connection_initiate(const struct shell* sh, size_t argc, char** argv);
static int cmd_blecon_announce(const struct shell* sh, size_t argc, char** argv);
static int cmd_blecon_scan(const struct shell* sh, size_t argc, char** argv);

// Shell command event handlers
static void cmd_blecon_connection_initiate_event(struct blecon_event_t* event, void* user_data);
static void cmd_blecon_announce_event(struct blecon_event_t* event, void* user_data);
static void cmd_blecon_scan_event(struct blecon_event_t* event, void* user_data);

// Event handler ids
static struct blecon_event_t* _cmd_blecon_connection_initiate_event = NULL;
static struct blecon_event_t* _cmd_blecon_announce_event = NULL;
static struct blecon_event_t* _cmd_blecon_scan_event = NULL;

void example_on_connection(struct blecon_t* blecon) {
    printk("Connected, sending request.\r\n");

    // Clean-up request
    blecon_request_cleanup(&_request);
    
    // Create message
    printk((char*)_outgoing_data_buffer, "Hello blecon!");

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

void example_on_scan_report(struct blecon_t* blecon) {}

void example_on_scan_complete(struct blecon_t* blecon) {
    printk("Scan complete\r\n");

    // Get results
    bool overflow = false;
    blecon_scan_get_data(blecon, 
        example_peer_scan_report_iterator, 
        example_raw_scan_report_iterator, 
        &overflow, NULL);

    if(overflow) {
        printk("Scan buffer overflowed\r\n");
    }
}

void example_on_raw_scan_complete(struct blecon_t* blecon) {

}

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

    printk("Frame: %s\r\n", message);

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

void example_raw_scan_report_iterator(const struct blecon_modem_raw_scan_report_t* report, void* user_data) {
    printk("Device spotted:");
    printk(" Address: ");
    char bt_addr_str[BLECON_BLUETOOTH_ADDR_SZ*2+1] = {0};
    blecon_util_append_hex_string(report->bt_addr.bytes, BLECON_BLUETOOTH_ADDR_SZ, bt_addr_str);
    printk("%s", bt_addr_str);
    printk(" Type: %" PRIu8, report->bt_addr.addr_type);
    if(report->is_scan_response) {
        printk(" (Scan)");
    }

    printk(" SID: %" PRIu8, report->sid);
    printk(" TX Power: %" PRIi8, report->tx_power);
    printk(" RSSI: %" PRIi8, report->rssi);

    printk(" Data: ");
    char* adv_data_str = calloc(report->adv_data_sz*2+1, sizeof(char));
    blecon_util_append_hex_string(report->adv_data, report->adv_data_sz, adv_data_str);
    printk("%s", adv_data_str);
    free(adv_data_str);
    printk("\r\n");
}

void example_peer_scan_report_iterator(const struct blecon_modem_peer_scan_report_t* report, void* user_data) {
    // Get UUID as string
    char uuid_str[BLECON_UUID_STR_SZ] = {0};
    blecon_util_append_uuid_string(report->blecon_id, uuid_str);
    
    printk("Peer spotted: ID: %s", uuid_str);
    printk(" TX power: %" PRIi8, report->tx_power);
    printk(" RSSI: %" PRIi8, report->rssi);
    if( report->is_announcing ) {
        printk(" **Announcing**");
    }
    printk("\r\n");
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
    _cmd_blecon_announce_event = blecon_event_loop_register_event(_event_loop, cmd_blecon_announce_event, NULL);
    _cmd_blecon_scan_event = blecon_event_loop_register_event(_event_loop, cmd_blecon_scan_event, NULL);

    // Blecon
    blecon_init(&_blecon, modem);
    blecon_set_callbacks(&_blecon, &blecon_callbacks, NULL);
    if(!blecon_setup(&_blecon)) {
        printk("Failed to setup blecon\r\n");
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
    if(_cmd_blecon_connection_initiate_event == NULL) {
        shell_print(sh, "Event not assigned");
        return -1;
    }
    blecon_event_signal(_cmd_blecon_connection_initiate_event);
    return 0;
}

int cmd_blecon_announce(const struct shell* sh, size_t argc, char** argv) {
    if(_cmd_blecon_announce_event == NULL) {
        shell_print(sh, "Event not assigned");
        return -1;
    }
    blecon_event_signal(_cmd_blecon_announce_event);
    return 0;
}

int cmd_blecon_scan(const struct shell* sh, size_t argc, char** argv) {
    if(_cmd_blecon_scan_event == NULL) {
        shell_print(sh, "Event not assigned");
        return -1;
    }
    blecon_event_signal(_cmd_blecon_scan_event);
    return 0;
}

void cmd_blecon_connection_initiate_event(struct blecon_event_t* event, void* user_data) {
    // Initiate connection
    if(!blecon_connection_initiate(&_blecon)) {
        printk("Failed to initiate connection\r\n");
        return;
    }
}

void cmd_blecon_announce_event(struct blecon_event_t* event, void* user_data) {
    // Announce device ID
    if(!blecon_announce(&_blecon)) {
        printk("Failed to announce device ID\r\n");
        return;
    }
}

void cmd_blecon_scan_event(struct blecon_event_t* event, void* user_data) {
    // Scan for nearby Blecon & Bluetooth devices
    if(!blecon_scan_start(&_blecon, true, true, blecon_scan_type_passive, 5000)) {
        printk("Failed to scan for nearby devices\r\n");
        return;
    }
}

SHELL_STATIC_SUBCMD_SET_CREATE(blecon,
	SHELL_CMD(connection_initiate, NULL, "Initiate connection to Blecon network.", cmd_blecon_connection_initiate),
    SHELL_CMD(announce, NULL, "Announce Blecon device ID to nearby hotspots.", cmd_blecon_announce),
    SHELL_CMD(scan, NULL, "Scan for nearby devices.", cmd_blecon_scan),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(blecon, &blecon, "Blecon commands", NULL);
