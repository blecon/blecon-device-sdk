/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "stdio.h"
#include "string.h"

#include "blecon/blecon.h"
#include "blecon_zephyr/blecon_zephyr_event_loop.h"
#include "blecon_zephyr/blecon_zephyr_bluetooth.h"
#include "blecon_zephyr/blecon_zephyr_crypto.h"
#include "blecon_zephyr/blecon_zephyr_nvm.h"
#include "blecon_zephyr/blecon_zephyr_nfc.h"

// Modem instance
static struct blecon_modem_t* _blecon_modem;

void blecon_on_connection(struct blecon_modem_t* modem, void* user_data) {
    printk("Connected, sending request.\r\n");
    static char message[64] = {0};
    sprintf(message, "Hello blecon!");

    enum blecon_ret_t ret = blecon_send_request(_blecon_modem, (const uint8_t*)message, strlen(message));
    if(ret != blecon_ok) {
        printk("Could not send request\r\n");
    }
}

void blecon_on_response(struct blecon_modem_t* modem, void* user_data) {
    char message[64] = {0};
    size_t message_sz = sizeof(message) - 1;
    enum blecon_ret_t ret = blecon_get_response(_blecon_modem, (uint8_t*)message, &message_sz);
    if(ret != blecon_ok) {
        printk("Could not get response\r\n");
        return;
    }

    message[message_sz] = '\0';

    printk("Got response: %s\r\n", message);
	blecon_close_connection(_blecon_modem); // Nothing left to send/receive
}

void blecon_on_error(struct blecon_modem_t* modem, void* user_data) {
    enum blecon_request_error_t request_error = blecon_request_ok;
    enum blecon_ret_t ret = blecon_get_error(_blecon_modem, &request_error);
    if(ret != blecon_ok) {
        printk("Could not get error\r\n");
        return;
    }
    printk("Got request error %x\r\n", request_error);
	blecon_close_connection(_blecon_modem); // Nothing left to send/receive
}

const static struct blecon_modem_callbacks_t blecon_modem_callbacks = {
	.on_connection = blecon_on_connection,
    .on_response = blecon_on_response,
    .on_error = blecon_on_error
};

void main(void)
{
#if defined(CONFIG_USB_CDC_ACM)
    // Give a chance to UART to connect
    k_sleep(K_MSEC(1000));
#endif
    
    // Init Event Loop
    struct blecon_event_loop_t* event_loop = blecon_zephyr_event_loop_new();

    // Init Bluetooth port
    struct blecon_bluetooth_t* bluetooth = blecon_zephyr_bluetooth_init(event_loop);

    // Init Crypto port
    struct blecon_crypto_t* crypto = blecon_zephyr_crypto_init();

    // Init NVM port
    struct blecon_nvm_t* nvm = blecon_zephyr_nvm_init();

    // Init NFC port
    struct blecon_nfc_t* nfc = blecon_zephyr_nfc_init();

	// Init modem
    _blecon_modem = blecon_int_modem_new(
        event_loop, bluetooth, crypto, nvm, nfc
    );

    // Set modem callbacks
    blecon_set_callbacks(_blecon_modem, &blecon_modem_callbacks, NULL);

    // Setup modem
    enum blecon_ret_t ret = blecon_setup(_blecon_modem);
    if(ret != blecon_ok) {
        printk("Could not setup modem\r\n");
        k_panic();
    }

    {
        // Print device URL
        char blecon_url[BLECON_URL_SZ] = {0};
        ret = blecon_get_url(_blecon_modem, blecon_url, sizeof(blecon_url));
        printk("Device URL: %s\r\n", blecon_url);
    }

    // Request connection
    ret = blecon_request_connection(_blecon_modem);
    if(ret != blecon_ok) {
        printk("Could not request connection\r\n");
        k_panic();
    }

	// Enter main loop.
    blecon_event_loop_run(event_loop);
}
