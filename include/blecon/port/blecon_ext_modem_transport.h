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

#include "blecon/blecon_error.h"

struct blecon_ext_modem_transport_t;

typedef void (*blecon_ext_modem_transport_signal_callback_t)(struct blecon_ext_modem_transport_t* transport, void* user_data);

struct blecon_ext_modem_transport_writer_t;
typedef bool (*blecon_ext_modem_transport_writer_write_fn_t)(struct blecon_ext_modem_transport_writer_t* writer, const uint8_t* data, size_t sz);

struct blecon_ext_modem_transport_writer_t {
    blecon_ext_modem_transport_writer_write_fn_t write;
    size_t remaining_sz;
};

struct blecon_ext_modem_transport_tx_frame_t {
    size_t (*get_size)(struct blecon_ext_modem_transport_tx_frame_t* frame);
    bool (*write)(struct blecon_ext_modem_transport_tx_frame_t* frame, struct blecon_ext_modem_transport_writer_t* writer);
};

struct blecon_ext_modem_transport_reader_t;
typedef bool (*blecon_ext_modem_transport_reader_read_fn_t)(struct blecon_ext_modem_transport_reader_t* reader, uint8_t* data, size_t sz);

struct blecon_ext_modem_transport_reader_t {
    blecon_ext_modem_transport_reader_read_fn_t read;
    size_t remaining_sz;
};

struct blecon_ext_modem_transport_rx_frame_t {
    bool (*set_size)(struct blecon_ext_modem_transport_rx_frame_t* frame, size_t sz);
    bool (*read)(struct blecon_ext_modem_transport_rx_frame_t* frame, struct blecon_ext_modem_transport_reader_t* reader);
};

struct blecon_ext_modem_transport_fn_t {
    void (*setup)(struct blecon_ext_modem_transport_t* transport);
    bool (*exchange_frames)(struct blecon_ext_modem_transport_t* transport, 
        struct blecon_ext_modem_transport_tx_frame_t* tx_frame,
        struct blecon_ext_modem_transport_rx_frame_t* rx_frame,
        bool* event
    );
};

struct blecon_ext_modem_transport_t {
    struct blecon_ext_modem_t* ext_modem;
    const struct blecon_ext_modem_transport_fn_t* fns;
    blecon_ext_modem_transport_signal_callback_t signal_callback;
    void* signal_callback_user_data;
};

static inline void blecon_ext_modem_transport_writer_init(struct blecon_ext_modem_transport_writer_t* writer, blecon_ext_modem_transport_writer_write_fn_t write_fn) {
    writer->write = write_fn;
    writer->remaining_sz = 0;
}

static inline void blecon_ext_modem_transport_writer_set_remaining_sz(struct blecon_ext_modem_transport_writer_t* writer, size_t sz) {
    writer->remaining_sz = sz;
}

static inline size_t blecon_ext_modem_transport_writer_get_remaining_sz(struct blecon_ext_modem_transport_writer_t* writer) {
    return writer->remaining_sz;
}

static inline bool blecon_ext_modem_transport_writer_write(struct blecon_ext_modem_transport_writer_t* writer, const uint8_t* data, size_t sz) {
    if(writer->remaining_sz < sz) {
        return false;
    }
    writer->remaining_sz -= sz;
    return writer->write(writer, data, sz);
}

static inline void blecon_ext_modem_transport_writer_assert_done(struct blecon_ext_modem_transport_writer_t* writer) {
    blecon_assert(writer->remaining_sz == 0);
}

static inline size_t blecon_ext_modem_transport_tx_frame_get_size(struct blecon_ext_modem_transport_tx_frame_t* frame) {
    return frame->get_size(frame);
}

static inline bool blecon_ext_modem_transport_tx_frame_write(struct blecon_ext_modem_transport_tx_frame_t* frame, struct blecon_ext_modem_transport_writer_t* writer) {
    return frame->write(frame, writer);
}

static inline void blecon_ext_modem_transport_reader_init(struct blecon_ext_modem_transport_reader_t* reader, blecon_ext_modem_transport_reader_read_fn_t read_fn) {
    reader->read = read_fn;
    reader->remaining_sz = 0;
}

static inline void blecon_ext_modem_transport_reader_set_remaining_sz(struct blecon_ext_modem_transport_reader_t* reader, size_t sz) {
    reader->remaining_sz = sz;
}

static inline size_t blecon_ext_modem_transport_reader_get_remaining_sz(struct blecon_ext_modem_transport_reader_t* reader) {
    return reader->remaining_sz;
}

static inline bool blecon_ext_modem_transport_reader_read(struct blecon_ext_modem_transport_reader_t* reader, uint8_t* data, size_t sz) {
    if(reader->remaining_sz < sz) {
        return false;
    }
    reader->remaining_sz -= sz;
    return reader->read(reader, data, sz);
}

static inline void blecon_ext_modem_transport_reader_assert_done(struct blecon_ext_modem_transport_reader_t* reader) {
    blecon_assert(reader->remaining_sz == 0);
}

static inline bool blecon_ext_modem_transport_rx_frame_set_size(struct blecon_ext_modem_transport_rx_frame_t* frame, size_t sz) {
    return frame->set_size(frame, sz);
}

static inline bool blecon_ext_modem_transport_rx_frame_read(struct blecon_ext_modem_transport_rx_frame_t* frame, struct blecon_ext_modem_transport_reader_t* reader) {
    return frame->read(frame, reader);
}

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
    struct blecon_ext_modem_transport_tx_frame_t* tx_frame,
    struct blecon_ext_modem_transport_rx_frame_t* rx_frame,
    bool* event
) {
    return transport->fns->exchange_frames(transport, 
        tx_frame, rx_frame, event
    );
}

static inline void blecon_ext_modem_transport_signal(struct blecon_ext_modem_transport_t* transport) {
    transport->signal_callback(transport, transport->signal_callback_user_data);
}

#ifdef __cplusplus
}
#endif
