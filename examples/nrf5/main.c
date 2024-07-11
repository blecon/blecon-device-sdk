/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_sdm.h"
#include "app_error.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_power.h"
#include "app_timer.h"
#include "app_scheduler.h"
#include "nrf_drv_clock.h"
#include "nrf_drv_power.h"
#include "app_uart.h"
#if defined (UART_PRESENT)
#include "nrf_uart.h"
#endif
#if defined (UARTE_PRESENT)
#include "nrf_uarte.h"
#endif
#include "bsp.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "blecon/blecon.h"
#include "blecon/blecon_error.h"

#include "blecon_nrf5/blecon_nrf5_bluetooth.h"
#include "blecon_nrf5/blecon_nrf5_crypto.h"
#include "blecon_nrf5/blecon_nrf5_event_loop.h"
#include "blecon_nrf5/blecon_nrf5_nfc.h"
#include "blecon_nrf5/blecon_nrf5_nvm.h"

#include "blecon_app.h"

// Note: BLE events are empty
#define SCHED_MAX_EVENT_DATA_SIZE       APP_TIMER_SCHED_EVENT_DATA_SIZE             /**< Maximum size of scheduler events. */
#define SCHED_QUEUE_SIZE                10                                          /**< Maximum number of events in the scheduler queue. More is needed in case of Serialization. */
#define DEAD_BEEF                           0xDEADBEEF 

// Handle fatal errors from Blecon lib
void blecon_fatal_error(void) {
    APP_ERROR_HANDLER(0);
}

// Handle softdevice asserts
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

#define UART_TX_BUF_SIZE 4096
#define UART_RX_BUF_SIZE 256

extern const nrf_log_backend_api_t nrf_log_backend_stdout_api;

NRF_LOG_BACKEND_DEF(stdout_log_backend, nrf_log_backend_stdout_api, NULL);

static void uart_error_handle(app_uart_evt_t * p_event)
{
    if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_communication);
    }
    else if (p_event->evt_type == APP_UART_FIFO_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
}

static void clock_power_init(void)
{
    ret_code_t err_code = nrf_drv_clock_init();
    if (err_code != NRF_ERROR_MODULE_ALREADY_INITIALIZED)
    {
        APP_ERROR_CHECK(err_code);
    }

    const static nrf_drv_power_config_t power_config = {
      .dcdcen = 1,
      #if NRF_POWER_HAS_VDDH
      .dcdcenhv = 1, // Only nRF52840/33
      #endif  
    };

    err_code = nrf_drv_power_init(&power_config);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
    ret_code_t err_code;

    // Initialize timer module.
    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the Event Scheduler initialization.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    int32_t backend_id = nrf_log_backend_add(&stdout_log_backend, NRF_LOG_SEVERITY_DEBUG);
    ASSERT(backend_id >= 0);
    nrf_log_backend_enable(&stdout_log_backend);
}

/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for application main entry.
 */
int main(void)
{
    log_init();

    clock_power_init();
    timers_init();
    power_management_init();
    bsp_board_init(BSP_INIT_LEDS);

    const app_uart_comm_params_t comm_params =
      {
          RX_PIN_NUMBER,
          TX_PIN_NUMBER,
          RTS_PIN_NUMBER,
          CTS_PIN_NUMBER,
          APP_UART_FLOW_CONTROL_DISABLED,
          false,
#if defined (UART_PRESENT)
          NRF_UART_BAUDRATE_115200
#else
          NRF_UARTE_BAUDRATE_115200
#endif
      };

    ret_code_t err_code = 0;
    APP_UART_FIFO_INIT(&comm_params,
                         UART_RX_BUF_SIZE,
                         UART_TX_BUF_SIZE,
                         uart_error_handle,
                         APP_IRQ_PRIORITY_LOWEST,
                         err_code);

    APP_ERROR_CHECK(err_code);

    scheduler_init();

    // Initialize app
    blecon_app_init();

    // Event loop
    struct blecon_event_loop_t* event_loop = blecon_nrf5_event_loop_init();
    
    // Bluetooth
    struct blecon_bluetooth_t* bluetooth = blecon_nrf5_bluetooth_init();

    // Crypto
    struct blecon_crypto_t* crypto = blecon_nrf5_crypto_init();

    // NVM
    struct blecon_nvm_t* nvm = blecon_nrf5_nvm_init();

    // NFC
    struct blecon_nfc_t* nfc = blecon_nrf5_nfc_init();

        // Init internal modem
    struct blecon_modem_t* modem = blecon_int_modem_create(
        event_loop,
        bluetooth,
        crypto,
        nvm,
        nfc,
        malloc
    );

    // Start app
    blecon_app_start(modem);

    // Enter main loop.
    blecon_event_loop_run(event_loop);
}


