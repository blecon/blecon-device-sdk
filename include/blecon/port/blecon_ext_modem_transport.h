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
#include "stdbool.h"

struct blecon_ext_modem_transport_t;

typedef void (*blecon_ext_modem_transport_signal_callback_t)(struct blecon_ext_modem_transport_t* transport, void* user_data);

struct blecon_ext_modem_transport_fn_t {
    void (*setup)(struct blecon_ext_modem_transport_t* transport);
    bool (*exchange_frames)(struct blecon_ext_modem_transport_t* transport, 
        const uint8_t* tx_header_buf, size_t tx_header_sz,
        const uint8_t* tx_data_buf, size_t tx_data_sz,
        uint8_t* rx_header_buf, size_t rx_header_sz,
        uint8_t* rx_data_buf, size_t max_rx_data_sz,
        size_t *rx_sz,
        bool* event
    );
};

struct blecon_ext_modem_transport_t {
    struct blecon_ext_modem_t* ext_modem;
    const struct blecon_ext_modem_transport_fn_t* fns;
    blecon_ext_modem_transport_signal_callback_t signal_callback;
    void* signal_callback_user_data;
};

static inline void blecon_ext_modem_transport_init(struct blecon_ext_modem_transport_t* transport, const struct blecon_ext_modem_transport_fn_t* fns) {
    transport->ext_modem = NULL;
    transport->fns = fns;
}

static inline void blecon_ext_modem_transport_setup(struct blecon_ext_modem_transport_t* transport, blecon_ext_modem_transport_signal_callback_t signal_callback, void* signal_callback_user_data) {
    transport->signal_callback = signal_callback;
    transport->signal_callback_user_data = signal_callback_user_data;
    transport->fns->setup(transport);
}

static inline bool blecon_ext_modem_transport_exchange_frames(struct blecon_ext_modem_transport_t* transport, 
    const uint8_t* tx_header_buf, size_t tx_header_sz,
    const uint8_t* tx_data_buf, size_t tx_data_sz,
    uint8_t* rx_header_buf, size_t rx_header_sz,
    uint8_t* rx_data_buf, size_t max_rx_data_sz,
    size_t *rx_sz,
    bool* event
) {
    return transport->fns->exchange_frames(transport, 
        tx_header_buf, tx_header_sz, tx_data_buf, tx_data_sz,
        rx_header_buf, rx_header_sz, rx_data_buf, max_rx_data_sz,
        rx_sz, event
    );
}

static inline void blecon_ext_modem_transport_signal(struct blecon_ext_modem_transport_t* transport) {
    transport->signal_callback(transport, transport->signal_callback_user_data);
}

#ifdef __cplusplus
}
#endif
