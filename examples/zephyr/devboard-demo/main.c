/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/input/input.h>
#include <zephyr/bluetooth/conn.h>

#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "unistd.h"

#include "blecon/blecon.h"
#include "blecon/blecon_error.h"
#include "blecon/blecon_util.h"
#include "blecon_zephyr/blecon_zephyr.h"
#include "blecon_zephyr/blecon_zephyr_event_loop.h"

#include "blecon_led.h"

#define PING_TIMEOUT  (60 * 1000)
#define REQUEST_CMD_DATA_MAX_LEN (256)

static struct blecon_event_loop_t* _event_loop = NULL;
static struct blecon_t _blecon = {0};
static struct blecon_request_t _request = {0};
static struct blecon_request_send_data_op_t _send_op = {0};
static struct blecon_request_receive_data_op_t _receive_op = {0};

static uint8_t _outgoing_data_buffer[REQUEST_CMD_DATA_MAX_LEN] = {0};
static uint8_t _incoming_data_buffer[64] = {0};
static bool _making_request = false;

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
static int cmd_blecon_request(const struct shell* sh, size_t argc, char** argv);
static int cmd_blecon_announce(const struct shell* sh, size_t argc, char** argv);
static int cmd_blecon_ping(const struct shell* sh, size_t argc, char** argv);
static int cmd_blecon_get_identity(const struct shell* sh, size_t argc, char** argv);
static int cmd_blecon_get_device_url(const struct shell* sh, size_t argc, char** argv);

// Shell command event handlers
static void cmd_blecon_request_event(struct blecon_event_t* event, void* user_data);
static void cmd_blecon_announce_event(struct blecon_event_t* event, void* user_data);
static void cmd_blecon_ping_event(struct blecon_event_t* event, void* user_data);
static void cmd_blecon_get_identity_event(struct blecon_event_t* event, void* user_data);
static void cmd_blecon_get_device_url_event(struct blecon_event_t* event, void* user_data);

// Input event handler
static void devboard_demo_on_input(struct input_event *evt, void* user_data);
static void announce_timeout(struct k_timer *);

// Zephyr Bluetooth status callbacks
static void zephyr_bt_connected(struct bt_conn *conn, uint8_t);
static void zephyr_bt_disconnected(struct bt_conn *conn, uint8_t);

// Event handlers
static struct blecon_event_t* _cmd_blecon_request_event = NULL;
static struct blecon_event_t* _cmd_blecon_announce_event = NULL;
static struct blecon_event_t* _cmd_blecon_ping_event = NULL;
static struct blecon_event_t* _cmd_blecon_get_identity_event = NULL;
static struct blecon_event_t* _cmd_blecon_get_device_url_event = NULL;

// Request work item
struct cmd_request_params {
    char namespace[BLECON_NAMESPACE_MAX_SZ];
    char method[BLECON_METHOD_MAX_SZ];
    char data[REQUEST_CMD_DATA_MAX_LEN];
};

static struct cmd_request_params _cmd_request_params = {0};

K_TIMER_DEFINE(announce_timer, announce_timeout, NULL);
INPUT_CALLBACK_DEFINE(NULL, devboard_demo_on_input, NULL);

void example_on_connection(struct blecon_t* blecon) {
    if(!_making_request) {
        return;
    }
    shell_print(shell_backend_uart_get_ptr(), "Connected, sending request.");

    // Clean-up (reset) request
    blecon_request_cleanup(&_request);

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
    _making_request = false;
}

void example_on_disconnection(struct blecon_t* blecon) {
}

void example_on_time_update(struct blecon_t* blecon) {
}

void example_on_ping_result(struct blecon_t* blecon) {
    bool latency_available;
    uint32_t connection_latency_ms;
    uint32_t rtt_ms;

    if(!blecon_ping_get_latency(blecon, &latency_available, &connection_latency_ms, &rtt_ms)) {
        printk("Error retrieving ping results\r\n");
        return;
    }

    if (!latency_available) {
        printk("Ping stats not available\r\n");
        return;
    }

    shell_print(shell_backend_uart_get_ptr(), "Connection time: %u ms, Round-trip time: %u ms", connection_latency_ms, rtt_ms);
}

void example_request_on_data_received(struct blecon_request_receive_data_op_t* receive_data_op, bool data_received, const uint8_t* data, size_t sz, bool finished) {
    // Retrieve response
    if(!data_received) {
        printk("Failed to receive data\r\n");
        return;
    }
    blecon_led_data_activity();

    static char message[REQUEST_CMD_DATA_MAX_LEN+1] = {0};
    memcpy(message, data, sz);
    message[sz] = '\0';

    shell_fprintf_normal(shell_backend_uart_get_ptr(), "%s", message);
}

void example_request_on_data_sent(struct blecon_request_send_data_op_t* send_data_op, bool data_sent) {
    if(!data_sent) {
        printk("Failed to send data\r\n");
        return;
    }
    blecon_led_data_activity();

    shell_print(shell_backend_uart_get_ptr(), "Data sent");
    shell_print(shell_backend_uart_get_ptr(), "Response:");
}

uint8_t* example_request_alloc_incoming_data_buffer(struct blecon_request_receive_data_op_t* receive_data_op, size_t sz) {
    return _incoming_data_buffer;
}

void example_request_on_closed(struct blecon_request_t* request) {
    enum blecon_request_status_code_t status_code = blecon_request_get_status(request);

    if(status_code != blecon_request_status_ok) {
        printk("Request failed with status code: %d\r\n", status_code);
    }
    else{
        shell_print(shell_backend_uart_get_ptr(), "\r\nRequest complete");
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

    // LEDs
    const struct device *leds = DEVICE_DT_GET(DT_PATH(leds));
    if (!device_is_ready(leds)) {
        printk("Device %s is not ready\n", leds->name);
        return 1;
    }
    blecon_led_init(leds, DT_NODE_CHILD_IDX(DT_ALIAS(led0)));

    // Personalise the shell
    char prompt[30];
    snprintf(prompt, sizeof(prompt), "%s:~$ ", CONFIG_BOARD);
    shell_prompt_change(shell_backend_uart_get_ptr(), prompt);

    // Register for Zephyr bluetooth status callbacks
    struct bt_conn_cb connected_cb = {0};
    connected_cb.connected = zephyr_bt_connected;
    connected_cb.disconnected = zephyr_bt_disconnected;
    if(bt_conn_cb_register(&connected_cb) != 0) {
        printk("Could not register for bluetooth status callbacks\r\n");
        return 1;
    }

    // Get modem
    struct blecon_modem_t* modem = blecon_zephyr_get_modem();

    // Register event ids for shell commands
    _cmd_blecon_request_event = blecon_event_loop_register_event(_event_loop, cmd_blecon_request_event, NULL);
    _cmd_blecon_announce_event = blecon_event_loop_register_event(_event_loop, cmd_blecon_announce_event, NULL);
    _cmd_blecon_ping_event = blecon_event_loop_register_event(_event_loop, cmd_blecon_ping_event, NULL);
    _cmd_blecon_get_identity_event = blecon_event_loop_register_event(_event_loop, cmd_blecon_get_identity_event, NULL);
    _cmd_blecon_get_device_url_event = blecon_event_loop_register_event(_event_loop, cmd_blecon_get_device_url_event, NULL);

    // Blecon
    blecon_init(&_blecon, modem);
    blecon_set_callbacks(&_blecon, &blecon_callbacks, NULL);
    if(!blecon_setup(&_blecon)) {
        printk("Failed to setup blecon\r\n");
        return 1;
    }

    // Enter main loop.
    blecon_event_loop_run(_event_loop);

    // Won't reach here
    return 0;
}

void devboard_demo_on_input(struct input_event *evt, void* user_data) {
    switch (evt->code) {
        case INPUT_KEY_0:
            if(evt->value == 1) {
                shell_execute_cmd(shell_backend_uart_get_ptr(), "blecon ping");
            }
            break;
        case INPUT_KEY_1:
            if(evt->value == 1) {
                shell_execute_cmd(shell_backend_uart_get_ptr(), "blecon announce");
            }
            break;
        case INPUT_KEY_2:
            if(evt->value == 1) {
                blecon_led_data_activity();
            }
            break;
        case INPUT_KEY_3:
            break;
        default:
            break;
    }
}

void announce_timeout(struct k_timer *timer) {
    blecon_led_set_announce(false);
}

void zephyr_bt_connected(struct bt_conn *conn, uint8_t) {
    blecon_led_set_connection_state(blecon_led_connection_state_connected);
}

void zephyr_bt_disconnected(struct bt_conn *conn, uint8_t) {
    blecon_led_set_connection_state(blecon_led_connection_state_disconnected);
}

int cmd_blecon_request(const struct shell* sh, size_t argc, char** argv) {
    int8_t c;

    // Set defaults
    strcpy(_cmd_request_params.namespace, "global:blecon_util");
    strcpy(_cmd_request_params.method, "echo");
    snprintf(_cmd_request_params.data, sizeof(_cmd_request_params.data), "Hello from %s", CONFIG_BOARD);

    while (true) {
        c = getopt(argc, argv, "n:m:");

        if(c == -1) {
            break;
        }

        switch (c) {
            case 'n':
                    strncpy(_cmd_request_params.namespace, optarg, BLECON_NAMESPACE_MAX_SZ-1);
                    break;
            case 'm':
                    strncpy(_cmd_request_params.method, optarg, BLECON_NAMESPACE_MAX_SZ-1);
                    break;
            default:
                    break;
        }
    }

    size_t len = 0;
    for(size_t i = optind; i < argc; i++) {
        if(i == optind) {
            _cmd_request_params.data[0] = '\0';
        }
        else {
            strncat(_cmd_request_params.data, " ", sizeof(_cmd_request_params.data) - len - 1);
            len += 1;
        }

        strncat(_cmd_request_params.data, argv[i], sizeof(_cmd_request_params.data) - len - 1);
        len += strlen(argv[i]);

        if (len >= sizeof(_cmd_request_params.data)-1) {
            break;
        }
    }

    if(_cmd_blecon_request_event == NULL) {
        shell_print(sh, "Event not assigned");
        return -1;
    }

    blecon_event_signal(_cmd_blecon_request_event);
    return 0;
}

int cmd_blecon_announce(const struct shell* sh, size_t argc, char** argv) {
    if(_cmd_blecon_announce_event == NULL) {
        shell_print(sh, "Event not assigned");
        return -1;
    }
    shell_print(sh, "Announcing device ID");
    blecon_event_signal(_cmd_blecon_announce_event);
    return 0;
}

int cmd_blecon_ping(const struct shell* sh, size_t argc, char** argv) {
    if(_cmd_blecon_announce_event == NULL) {
        shell_print(sh, "Event not assigned");
        return -1;
    }
    shell_print(sh, "Starting ping");
    blecon_event_signal(_cmd_blecon_ping_event);
    return 0;
}

int cmd_blecon_get_identity(const struct shell* sh, size_t argc, char** argv) {
    if(_cmd_blecon_get_identity_event == NULL) {
        shell_print(sh, "Event not assigned");
        return -1;
    }
    blecon_event_signal(_cmd_blecon_get_identity_event);
    return 0;
}

int cmd_blecon_get_device_url(const struct shell* sh, size_t argc, char** argv) {
    if(_cmd_blecon_get_device_url_event == NULL) {
        shell_print(sh, "Event not assigned");
        return -1;
    }
    blecon_event_signal(_cmd_blecon_get_device_url_event);
    return 0;
}

void cmd_blecon_request_event(struct blecon_event_t* event, void* user_data) {
    _making_request = true;

    // Init request
    static struct blecon_request_parameters_t request_params = {
        .namespace = _cmd_request_params.namespace,
        .method = _cmd_request_params.method,
        .oneway = false,
        .request_content_type = "text/plain",
        .response_content_type = "text/plain",
        .response_mtu = sizeof(_incoming_data_buffer),
        .callbacks = &blecon_request_callbacks,
        .user_data = NULL
    };

    strncpy(_outgoing_data_buffer, _cmd_request_params.data, REQUEST_CMD_DATA_MAX_LEN);

    blecon_request_init(&_request, &request_params);

    if(!blecon_connection_initiate(&_blecon)) {
        printk("Failed to initiate connection\r\n");
        return;
    }
    blecon_led_set_connection_state(blecon_led_connection_state_connecting);
}

void cmd_blecon_announce_event(struct blecon_event_t* event, void* user_data) {
    // Announce device ID
    if(!blecon_announce(&_blecon)) {
        printk("Failed to announce device ID\r\n");
        return;
    }
    blecon_led_set_announce(true);
    k_timer_start(&announce_timer, K_SECONDS(5), K_FOREVER);
}

void cmd_blecon_ping_event(struct blecon_event_t* event, void* user_data) {
    // Run a ping
    if(!blecon_ping_perform(&_blecon, PING_TIMEOUT)) {
        printk("Failed to ping\r\n");
        return;
    }
    blecon_led_set_connection_state(blecon_led_connection_state_connecting);
}

void cmd_blecon_get_identity_event(struct blecon_event_t* event, void* user_data) {
    uint8_t device_id[BLECON_UUID_SZ];
    char device_id_str[BLECON_UUID_STR_SZ];

    if(!blecon_get_identity(&_blecon, device_id)) {
        printk("Could not get device identity\r\n");
        return;
    }
    blecon_util_append_uuid_string(device_id, device_id_str);
    shell_print(shell_backend_uart_get_ptr(), "Device ID: %s", device_id_str);
}

void cmd_blecon_get_device_url_event(struct blecon_event_t* event, void* user_data) {
    char blecon_url[BLECON_URL_SZ];
    if(!(blecon_get_url(&_blecon, blecon_url, sizeof(blecon_url)))) {
        printk("Failed to get device URL\r\n");
        return;
    }
    shell_print(shell_backend_uart_get_ptr(), "Device URL: %s", blecon_url);
}

SHELL_STATIC_SUBCMD_SET_CREATE(blecon,
	SHELL_CMD(request, NULL, "Send a request", cmd_blecon_request),
    SHELL_CMD(announce, NULL, "Announce Blecon device ID to nearby hotspots.", cmd_blecon_announce),
    SHELL_CMD(ping, NULL, "Ping Blecon service.", cmd_blecon_ping),
    SHELL_CMD(get-device-id, NULL, "Print this device's ID.", cmd_blecon_get_identity),
    SHELL_CMD(get-device-url, NULL, "Print this device's ID as a URL.", cmd_blecon_get_device_url),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(blecon, &blecon, "Blecon commands", NULL);
