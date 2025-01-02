/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blecon_nrf5_timer.h"
#include "blecon/blecon_scheduler.h"
#include "blecon/blecon_error.h"

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_sdm.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_pwr_mgmt.h"
#include "app_timer.h"
#include "app_scheduler.h"

// Validate nRF5 SDK Config
#if !APP_TIMER_ENABLED || APP_TIMER_CONFIG_USE_SCHEDULER
#error "APP_TIMER_ENABLED must be enabled and APP_TIMER_CONFIG_USE_SCHEDULER disabled in sdk_config.h"
#endif

static void blecon_nrf5_timer_setup(struct blecon_timer_t* timer);
static uint64_t blecon_nrf5_timer_get_monotonic_time(struct blecon_timer_t* timer);
static void blecon_nrf5_timer_set_timeout(struct blecon_timer_t* timer, uint32_t timeout_ms);
static void blecon_nrf5_timer_cancel_timeout(struct blecon_timer_t* timer);

static void blecon_nrf5_timeout_handler(void* p_context);
static void blecon_nrf5_monotonic_time_offset_monitor_handler(void* p_context);
static void blecon_nrf5_update_monotonic_ticks(void);

#define RTC_BITS 24ul
#define MAX_RTC_COUNTER_VAL 0xFFFFFFul

APP_TIMER_DEF(_timeout_timer_id);
APP_TIMER_DEF(_monotonic_ticks_offset_monitor_timer_id);
static struct blecon_timer_t _timer;
static uint64_t _monotonic_ticks;
static uint32_t _rtc_ticks_offset;

struct blecon_timer_t* blecon_nrf5_timer_init(void) {
    static const struct blecon_timer_fn_t timer_fn = {
        .setup = blecon_nrf5_timer_setup,
        .get_monotonic_time = blecon_nrf5_timer_get_monotonic_time,
        .set_timeout = blecon_nrf5_timer_set_timeout,
        .cancel_timeout = blecon_nrf5_timer_cancel_timeout
    };

    blecon_timer_init(&_timer, &timer_fn);

    _monotonic_ticks = 0;
    _rtc_ticks_offset = 0;

    return &_timer;
}

void blecon_nrf5_timer_setup(struct blecon_timer_t* timer) {
    // Create timers
    ret_code_t err_code = app_timer_create(&_timeout_timer_id,
        APP_TIMER_MODE_SINGLE_SHOT, blecon_nrf5_timeout_handler);
    blecon_assert(err_code == NRF_SUCCESS);

    err_code = app_timer_create(&_monotonic_ticks_offset_monitor_timer_id,
        APP_TIMER_MODE_REPEATED, blecon_nrf5_monotonic_time_offset_monitor_handler);
    blecon_assert(err_code == NRF_SUCCESS);

    // Set initial offset
    _rtc_ticks_offset = app_timer_cnt_get();

    // Start monotonic ticks timer
    err_code = app_timer_start(_monotonic_ticks_offset_monitor_timer_id, MAX_RTC_COUNTER_VAL >> 1ul, NULL);
    blecon_assert(err_code == NRF_SUCCESS);
}

#define RTC_FREQUENCY_DIVISOR (15ul - APP_TIMER_CONFIG_RTC_FREQUENCY)

uint64_t blecon_nrf5_timer_get_monotonic_time(struct blecon_timer_t* timer) {
    blecon_nrf5_update_monotonic_ticks();
    
    uint64_t time =  _monotonic_ticks >> RTC_FREQUENCY_DIVISOR; // Divide by RTC frequency (+ prescaler) to get seconds
    uint32_t remainder = _monotonic_ticks & ~(UINT32_MAX << RTC_FREQUENCY_DIVISOR); // Get remainder
    time = time * 1000ul + ((remainder * 1000ul) >> RTC_FREQUENCY_DIVISOR);
    return time;
}

void blecon_nrf5_timer_set_timeout(struct blecon_timer_t* timer, uint32_t timeout_ms) {
    uint32_t ticks = 0;
    
    if(timeout_ms > (1000ul * MAX_RTC_COUNTER_VAL) / (APP_TIMER_CLOCK_FREQ / (1ul + APP_TIMER_CONFIG_RTC_FREQUENCY)) - 1) {
        // Maximum timeout
        ticks = MAX_RTC_COUNTER_VAL;
    } else {
        ticks = APP_TIMER_TICKS(timeout_ms);
    }
    
    if(ticks < 5) { // minimum ticks for timer api
        ticks = 5;
    }

    // Stop timer if running
    app_timer_stop(_timeout_timer_id);

    // Start timer
    ret_code_t err_code;
    err_code = app_timer_start(_timeout_timer_id, ticks, timer);
    blecon_assert(err_code == NRF_SUCCESS);
}

void blecon_nrf5_timer_cancel_timeout(struct blecon_timer_t* timer) {
    app_timer_stop(_timeout_timer_id);
}

void blecon_nrf5_timeout_handler(void* p_context) {
    struct blecon_timer_t* timer = (struct blecon_timer_t*) p_context;

    // Process handler
    blecon_timer_on_timeout(timer);
}

void blecon_nrf5_monotonic_time_offset_monitor_handler(void* p_context) {
    blecon_nrf5_update_monotonic_ticks();
}

// Internal functions
void blecon_nrf5_update_monotonic_ticks(void) {
    // Increment time offset
    uint32_t rtc_ticks = app_timer_cnt_get();
    uint32_t rtc_ticks_diff = app_timer_cnt_diff_compute(rtc_ticks, _rtc_ticks_offset);

    // Amend monotonic offset
    _monotonic_ticks += rtc_ticks_diff;

    // Sync offsets
    _rtc_ticks_offset = rtc_ticks;
}
