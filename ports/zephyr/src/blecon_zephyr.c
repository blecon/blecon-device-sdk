/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stdlib.h"
#include "string.h"

#include "blecon_zephyr/blecon_zephyr.h"
#include "blecon/blecon.h"
#include "blecon/blecon_error.h"

#include "blecon_zephyr/blecon_zephyr_event_loop.h"
#if defined(CONFIG_BLECON_INTERNAL_MODEM)
#include "blecon_zephyr/blecon_zephyr_timer.h"
#include "blecon_zephyr/blecon_zephyr_bluetooth.h"
#include "blecon_zephyr/blecon_zephyr_crypto.h"
#include "blecon_zephyr/blecon_zephyr_nvm.h"
#include "blecon_zephyr/blecon_zephyr_nfc.h"
#if defined(CONFIG_BLECON_PORT_OTP)
#include "blecon_zephyr/blecon_zephyr_otp.h"
#endif
#if defined(CONFIG_BLECON_TRACING)
#include "blecon_zephyr/blecon_zephyr_tracing.h"
#endif
#elif defined(CONFIG_BLECON_EXTERNAL_MODEM)
#include "blecon_zephyr/blecon_zephyr_ext_modem_uart_transport.h"
#endif

#if defined(CONFIG_BLECON_INTERNAL_MODEM) || defined(CONFIG_BLECON_EXTERNAL_MODEM)
// Allocator wrapper that ignores user_data and calls malloc
static void* blecon_zephyr_allocator(size_t size, void* user_data) {
    (void)user_data;
    return malloc(size);
}
#endif

static struct blecon_event_loop_t* _event_loop = NULL;

struct blecon_event_loop_t* blecon_zephyr_get_event_loop(void) {
    if( _event_loop != NULL ) {
        return _event_loop;
    }
    _event_loop = blecon_zephyr_event_loop_new();
    return _event_loop;
}

#if !CONFIG_BLECON_NO_MODEM
static struct blecon_modem_t* _modem = NULL;

struct blecon_modem_t* blecon_zephyr_get_modem(void) {
    if( _modem != NULL ) {
        return _modem;
    }

#if defined(CONFIG_BLECON_INTERNAL_MODEM)
    // Init Event Loop
    struct blecon_event_loop_t* event_loop = blecon_zephyr_get_event_loop();

    // Init Timer
    struct blecon_timer_t* timer = blecon_zephyr_timer_new();

    // Init Bluetooth port
    struct blecon_bluetooth_t* bluetooth = blecon_zephyr_bluetooth_init(_event_loop);

    // Init Crypto port
    struct blecon_crypto_t* crypto = blecon_zephyr_crypto_init();

    // Init NVM port
    struct blecon_nvm_t* nvm = blecon_zephyr_nvm_init();

    // Init NFC port
    struct blecon_nfc_t* nfc = blecon_zephyr_nfc_init();

    // Init OTP port
#if defined(CONFIG_BLECON_PORT_OTP)
    struct blecon_otp_t* otp = blecon_zephyr_otp_init();
#else
    struct blecon_otp_t* otp = NULL;
#endif

    // Init internal modem
    struct blecon_modem_t* modem = blecon_int_modem_create(
        event_loop,
        timer,
        bluetooth,
        crypto,
        nvm,
        otp,
        nfc,
        CONFIG_BLECON_INTERNAL_MODEM_SCAN_BUFFER_SIZE,
        blecon_zephyr_allocator,
        NULL
    );
#elif defined(CONFIG_BLECON_EXTERNAL_MODEM)
    // Init Event Loop
    struct blecon_event_loop_t* event_loop = blecon_zephyr_get_event_loop();

    // Init UART device
    const struct device* uart_device = DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(blecon_modem)));

    // Init external modem transport. Static: the event loop and the modem keep its address.
    static struct blecon_zephyr_ext_modem_uart_transport_t ext_modem_uart_transport;
    blecon_zephyr_ext_modem_uart_transport_init(&ext_modem_uart_transport, _event_loop, uart_device);

    // Init external modem
    struct blecon_modem_t* modem = blecon_ext_modem_create(
        event_loop,
        blecon_zephyr_ext_modem_uart_transport_as_transport(&ext_modem_uart_transport),
        blecon_zephyr_allocator,
        NULL
    );
#else
#error "No modem implementation selected - please enable a Blecon modem implementation in prj.conf"
#endif
    blecon_assert(modem != NULL); // Make sure allocation was successful
    _modem = modem;

#if defined(CONFIG_BLECON_TRACING)
    // Arm the factory-owned tracing channel and set up the selected trace transport, before
    // the app's blecon_setup() runs the one-shot channel setup. CONFIG_BLECON_TRACING depends
    // on the internal modem, so `timer` is the one created above. The modem is a
    // process-lifetime singleton that is never destroyed, so there is no teardown counterpart.
    blecon_zephyr_tracing_setup(_modem, timer);
#endif

    return _modem;
}
#endif
