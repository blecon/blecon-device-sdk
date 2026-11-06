/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdk_common.h"
#include "nrf_log_backend_serial.h"
#include "nrf_log_internal.h"

#include "app_error.h"

static uint8_t m_string_buff[4096];

static void stdout_write(void const * p_context, char const * p_buffer, size_t len)
{
    fwrite(p_buffer, 1, len, stdout);
}

static void nrf_log_backend_stdout_put(nrf_log_backend_t const * p_backend,
                                     nrf_log_entry_t * p_msg)
{
    nrf_log_backend_serial_put(p_backend, p_msg, m_string_buff,
                               sizeof(m_string_buff), stdout_write);
}

static void nrf_log_backend_stdout_flush(nrf_log_backend_t const * p_backend)
{
    fflush(stdout);
}

static void nrf_log_backend_stdout_panic_set(nrf_log_backend_t const * p_backend)
{
   
}

const nrf_log_backend_api_t nrf_log_backend_stdout_api = {
        .put       = nrf_log_backend_stdout_put,
        .flush     = nrf_log_backend_stdout_flush,
        .panic_set = nrf_log_backend_stdout_panic_set,
};
