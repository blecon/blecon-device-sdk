/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "app_scheduler.h"
#include "app_error.h"
#include "bsp.h"
#include "inttypes.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "blecon/blecon.h"
#include "blecon/blecon_error.h"
#include "blecon_app.h"
#include "blecon/blecon_util.h"

static struct blecon_t _blecon = {0};

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

// Scan report iterators
static void example_raw_scan_report_iterator(const struct blecon_modem_raw_scan_report_t* report, void* user_data);
static void example_peer_scan_report_iterator(const struct blecon_modem_peer_scan_report_t* report, void* user_data);


// Utility macro
static inline void blecon_check_error(enum blecon_ret_t code) { 
    if(code != blecon_ok) {
        NRF_LOG_ERROR("Blecon modem error: 0x%x\r\n", code);
        blecon_fatal_error();
    }
}

static void scan_from_irq(void * p_event_data, uint16_t event_size) {
    printf("Scanning.\r\n");
    
    // Scan for nearby Blecon & Bluetooth devices
    if(!blecon_scan_start(&_blecon, true, true, 5000)) {
        printf("Failed to scan for nearby devices\r\n");
        return;
    }

    bsp_board_leds_off();
    bsp_board_led_on(0);
}

// Handle BSP events
void bsp_event_handler(bsp_event_t event)
{
    switch (event)
    {
        case BSP_EVENT_KEY_0:
            // This is called in IRQ context, so put in scheduler's queue
            app_sched_event_put(NULL, 0, scan_from_irq);
            break;

        case BSP_EVENT_KEY_1:
            break;

        case BSP_EVENT_KEY_2:
            break;

        default:
            break;
    }
}

// Initialise buttons and LEDs
static void buttons_leds_init(void)
{
    ret_code_t err_code;

    err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);
}

void example_on_connection(struct blecon_t* blecon) {}

void example_on_disconnection(struct blecon_t* blecon) {}

void example_on_time_update(struct blecon_t* blecon) {}

void example_on_ping_result(struct blecon_t* blecon) {}

void example_on_scan_report(struct blecon_t* blecon) {}

void example_on_scan_complete(struct blecon_t* blecon) {
    printf("Scan complete\r\n");

    // Get results
    bool overflow = false;
    blecon_scan_get_data(blecon, 
        example_peer_scan_report_iterator, 
        example_raw_scan_report_iterator, 
        &overflow, NULL);

    if(overflow) {
        printf("Scan buffer overflowed\r\n");
    }
}

void example_raw_scan_report_iterator(const struct blecon_modem_raw_scan_report_t* report, void* user_data) {
    printf("Device spotted:");
    printf(" Address: ");
    char bt_addr_str[BLECON_BLUETOOTH_ADDR_SZ*2+1] = {0};
    blecon_util_append_hex_string(report->bt_addr.bytes, BLECON_BLUETOOTH_ADDR_SZ, bt_addr_str);
    printf("%s", bt_addr_str);
    printf(" Type: %" PRIu8, report->bt_addr.addr_type);

    printf(" SID: %" PRIu8, report->sid);
    printf(" TX Power: %" PRIi8, report->tx_power);
    printf(" RSSI: %" PRIi8, report->rssi);

    printf(" Data: ");
    char* adv_data_str = calloc(report->adv_data_sz*2+1, sizeof(char));
    blecon_util_append_hex_string(report->adv_data, report->adv_data_sz, adv_data_str);
    printf("%s", adv_data_str);
    free(adv_data_str);
    printf("\r\n");
}

void example_peer_scan_report_iterator(const struct blecon_modem_peer_scan_report_t* report, void* user_data) {
    // Get UUID as string
    char uuid_str[BLECON_UUID_STR_SZ] = {0};
    blecon_util_append_uuid_string(report->blecon_id, uuid_str);
    
    printf("Peer spotted: ID: %s", uuid_str);
    printf(" TX power: %" PRIi8, report->tx_power);
    printf(" RSSI: %" PRIi8, report->rssi);
    if( report->is_announcing ) {
        printf(" **Announcing**");
    }
    printf("\r\n");
}

void blecon_app_init(void) {
    // Initialise buttons and LEDs
    buttons_leds_init();
}

void blecon_app_start(struct blecon_modem_t* blecon_modem) {
    // Blecon
    blecon_init(&_blecon, blecon_modem);
    blecon_set_callbacks(&_blecon, &blecon_callbacks, NULL);
    if(!blecon_setup(&_blecon)) {
        printf("Failed to setup blecon\r\n");
        return;
    }

    // Print device URL
    char blecon_url[BLECON_URL_SZ] = {0};
    if(!blecon_get_url(&_blecon, blecon_url, sizeof(blecon_url))) {
        printf("Failed to get device URL\r\n");
        return;
    }
    NRF_LOG_INFO("Device URL: %s", NRF_LOG_PUSH(blecon_url));

    NRF_LOG_INFO("Press button 1 to scan for devices");
}
