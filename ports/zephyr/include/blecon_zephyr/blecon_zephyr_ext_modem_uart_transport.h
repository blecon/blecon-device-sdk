/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"
#include "stddef.h"
#include "blecon/port/blecon_ext_modem_transport.h"
#include "zephyr/sys/ring_buffer.h"
#include "zephyr/drivers/uart.h"

struct blecon_event_loop_t;

struct blecon_zephyr_ext_modem_uart_transport_t {
    struct blecon_ext_modem_transport_t ext_modem_transport;
    struct blecon_event_t* event;
    struct blecon_ext_modem_transport_writer_t writer;
    struct blecon_ext_modem_transport_reader_t reader;
    bool rx_frame_done;
    bool rx_timeout;
    const struct device* uart_device;
    bool rx_error;
    struct k_sem rx_sem;
    struct ring_buf rx_fifo;
    uint8_t rx_fifo_buf[CONFIG_BLECON_EXTERNAL_MODEM_UART_TRANSPORT_RX_FIFO_SZ];
};

void blecon_zephyr_ext_modem_uart_transport_init(struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport,
    struct blecon_event_loop_t* event_loop, const struct device* uart_device);

static inline struct blecon_ext_modem_transport_t* blecon_zephyr_ext_modem_uart_transport_as_transport(struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport) {
    return &ext_modem_uart_transport->ext_modem_transport;
}

#ifdef __cplusplus
}
#endif
