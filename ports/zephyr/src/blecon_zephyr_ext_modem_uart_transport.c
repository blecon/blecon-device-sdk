/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "blecon_zephyr_ext_modem_uart_transport.h"
#include "blecon_zephyr/blecon_zephyr_event_loop.h"

#define UART_TRANSPORT_DEFAULT_FIRST_CHAR_TIMEOUT_MS 200u
#define UART_TRANSPORT_DEFAULT_NEXT_CHARS_TIMEOUT_MS 10u

static void blecon_zephyr_ext_modem_uart_transport_setup(struct blecon_ext_modem_transport_t* transport);
static bool blecon_zephyr_ext_modem_uart_transport_exchange_frames(struct blecon_ext_modem_transport_t* transport, 
        struct blecon_ext_modem_transport_tx_frame_t* tx_frame,
        struct blecon_ext_modem_transport_rx_frame_t* rx_frame,
        bool* event
    );
static bool blecon_zephyr_ext_modem_uart_transport_writer_write(struct blecon_ext_modem_transport_writer_t* writer, const uint8_t* data, size_t sz);
static bool blecon_zephyr_ext_modem_uart_transport_reader_read(struct blecon_ext_modem_transport_reader_t* reader, uint8_t* data, size_t sz);

static bool blecon_zephyr_ext_modem_uart_transport_read_bytes(struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport, 
    uint8_t* buf, size_t* sz, uint32_t timeout_ms, bool* done);
static bool blecon_zephyr_ext_modem_uart_transport_read_char(struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport, uint32_t timeout_ms, char* c);

static bool blecon_zephyr_ext_modem_uart_transport_write_bytes(struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport, const uint8_t* data, size_t sz);
static bool blecon_zephyr_ext_modem_uart_transport_write_done(struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport);
static bool blecon_zephyr_ext_modem_uart_transport_write_char(struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport, char c);

static void blecon_zephyr_ext_modem_uart_transport_rx_event(struct blecon_event_t* event, void* user_data);

static void blecon_zephyr_ext_modem_uart_transport_interrupt_handler(const struct device* dev, void* user_data);

void blecon_zephyr_ext_modem_uart_transport_init(struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport,
    struct blecon_event_loop_t* event_loop, const struct device* uart_device) {
    const static struct blecon_ext_modem_transport_fn_t blecon_zephyr_ext_modem_uart_transport_functions = {
        .setup = blecon_zephyr_ext_modem_uart_transport_setup,
        .exchange_frames = blecon_zephyr_ext_modem_uart_transport_exchange_frames
    };

    // Initialise transport
    blecon_ext_modem_transport_init(&ext_modem_uart_transport->ext_modem_transport, &blecon_zephyr_ext_modem_uart_transport_functions);

    // Register event
    ext_modem_uart_transport->event = blecon_event_loop_register_event(event_loop, blecon_zephyr_ext_modem_uart_transport_rx_event, ext_modem_uart_transport);

    // Initialise transport writer
    blecon_ext_modem_transport_writer_init(&ext_modem_uart_transport->writer, blecon_zephyr_ext_modem_uart_transport_writer_write);

    // Initialise transport reader
    blecon_ext_modem_transport_reader_init(&ext_modem_uart_transport->reader, blecon_zephyr_ext_modem_uart_transport_reader_read);
    ext_modem_uart_transport->rx_frame_done = false;
    ext_modem_uart_transport->rx_timeout = false;

    // Store UART device
    ext_modem_uart_transport->uart_device = uart_device;
    
    // Initialise semaphore
    ext_modem_uart_transport->rx_error = false;
    k_sem_init(&ext_modem_uart_transport->rx_sem, 0, 1);

    // Init read buffer
    ring_buf_init(&ext_modem_uart_transport->rx_fifo, sizeof(ext_modem_uart_transport->rx_fifo_buf), ext_modem_uart_transport->rx_fifo_buf);
}

void blecon_zephyr_ext_modem_uart_transport_setup(struct blecon_ext_modem_transport_t* transport) {
    struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport = (struct blecon_zephyr_ext_modem_uart_transport_t*) transport;
    blecon_assert(device_is_ready(ext_modem_uart_transport->uart_device));

    // Set-up interrupt
    uart_irq_callback_user_data_set(ext_modem_uart_transport->uart_device, blecon_zephyr_ext_modem_uart_transport_interrupt_handler, ext_modem_uart_transport);
    uart_irq_rx_enable(ext_modem_uart_transport->uart_device);
}

bool blecon_zephyr_ext_modem_uart_transport_exchange_frames(struct blecon_ext_modem_transport_t* transport, 
        struct blecon_ext_modem_transport_tx_frame_t* tx_frame,
        struct blecon_ext_modem_transport_rx_frame_t* rx_frame,
        bool* event
    ) {
    struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport = (struct blecon_zephyr_ext_modem_uart_transport_t*) transport;
    size_t tx_sz = blecon_ext_modem_transport_tx_frame_get_size(tx_frame);
    uint8_t tx_frame_header[] = { 0x01, tx_sz & 0xff, (tx_sz >> 8u) & 0xff };

    // Write frame header
    bool success = blecon_zephyr_ext_modem_uart_transport_write_bytes(ext_modem_uart_transport, tx_frame_header, sizeof(tx_frame_header));
    if(!success) {
        return false;
    }

    // Write rest of frame
    blecon_ext_modem_transport_writer_set_remaining_sz(&ext_modem_uart_transport->writer, tx_sz);
    success = blecon_ext_modem_transport_tx_frame_write(tx_frame, &ext_modem_uart_transport->writer);
    if(!success) {
        return false;
    }
    blecon_ext_modem_transport_writer_assert_done(&ext_modem_uart_transport->writer);
    
    // Write frame done
    success = blecon_zephyr_ext_modem_uart_transport_write_done(ext_modem_uart_transport);
    if(!success) {
        return false;
    }

    // Reset RX flags
    ext_modem_uart_transport->rx_frame_done = false;
    ext_modem_uart_transport->rx_timeout = false;

    // Read frame header
    uint8_t rx_frame_header[3] = {0};
    do {
        size_t rx_read_sz = sizeof(rx_frame_header);
        bool rx_done = false;
        success = blecon_zephyr_ext_modem_uart_transport_read_bytes(ext_modem_uart_transport, rx_frame_header, &rx_read_sz, UART_TRANSPORT_DEFAULT_FIRST_CHAR_TIMEOUT_MS, &rx_done);
        if(!success) {
            return false;
        }

        if(rx_read_sz < 1) {
            return false;
        }

        if(rx_frame_header[0] & 0x02) {
            *event = true;
        }

        if( !(rx_frame_header[0] & 0x01) ) {
            if(rx_done) {
                continue; // Wait for next frame
            } else {
                return false; // Not a valid frame
            }
        }

        if(rx_read_sz < 3) {
            return false;
        }

        break;
    } while(true);

    size_t rx_sz = rx_frame_header[1] | ((size_t)rx_frame_header[2] << 8u);

    bool rx_error = false;
    if(!blecon_ext_modem_transport_rx_frame_set_size(rx_frame, rx_sz)) {
        rx_error = true;
        goto done;
    }

    // Read rest of the frame
    blecon_ext_modem_transport_reader_set_remaining_sz(&ext_modem_uart_transport->reader, rx_sz);
    success = blecon_ext_modem_transport_rx_frame_read(rx_frame, &ext_modem_uart_transport->reader);
    if(!success) {
        rx_error = true;
        goto done;
    }
    blecon_ext_modem_transport_reader_assert_done(&ext_modem_uart_transport->reader);

done:
    if(!ext_modem_uart_transport->rx_frame_done && !ext_modem_uart_transport->rx_timeout) {
        // Expect frame to be done (and discard any extra characters)
        uint8_t buf[16] = {0};
        bool rx_done = false;
        do {
            size_t read_sz = sizeof(buf);
            bool success = blecon_zephyr_ext_modem_uart_transport_read_bytes(ext_modem_uart_transport, buf, &read_sz, UART_TRANSPORT_DEFAULT_NEXT_CHARS_TIMEOUT_MS, &rx_done);
            if(!success) {
                return false;
            }
        } while(!rx_done);
    }

    return !rx_error;
}

bool blecon_zephyr_ext_modem_uart_transport_writer_write(struct blecon_ext_modem_transport_writer_t* writer, const uint8_t* data, size_t sz) {
    // Retrieve transport from writer
    struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport = (struct blecon_zephyr_ext_modem_uart_transport_t*) ((char*)writer - offsetof(struct blecon_zephyr_ext_modem_uart_transport_t, writer));

    return blecon_zephyr_ext_modem_uart_transport_write_bytes(ext_modem_uart_transport, data, sz);
}

bool blecon_zephyr_ext_modem_uart_transport_reader_read(struct blecon_ext_modem_transport_reader_t* reader, uint8_t* data, size_t sz) {
    // Retrieve transport from reader
    struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport = (struct blecon_zephyr_ext_modem_uart_transport_t*) ((char*)reader - offsetof(struct blecon_zephyr_ext_modem_uart_transport_t, reader));

    size_t read_sz = sz;
    bool done = false;
    bool success = blecon_zephyr_ext_modem_uart_transport_read_bytes(ext_modem_uart_transport, data, &read_sz, UART_TRANSPORT_DEFAULT_NEXT_CHARS_TIMEOUT_MS, &done);
    if(!success) {
        // Timeout
        ext_modem_uart_transport->rx_timeout = true;
        return false;
    }

    // Update RX frame done flag
    ext_modem_uart_transport->rx_frame_done = done;

    if(read_sz != sz) {
        return false;
    }

    // The done flag should never be set here
    if( done ) {
        return false;
    }

    return true;
}

bool blecon_zephyr_ext_modem_uart_transport_read_bytes(struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport, 
    uint8_t* buf, size_t* sz, uint32_t timeout_ms, bool* done) {

    bool first_char = true;
    bool wait_for_msb = true; // MSB first, LSB then
    size_t read_sz = 0;
    *done = false;

    do {
        if( read_sz >= *sz ) {
            break;
        }

        int timeout = first_char ? timeout_ms : UART_TRANSPORT_DEFAULT_NEXT_CHARS_TIMEOUT_MS;
        first_char = false;
        char c = 0;
        bool success = blecon_zephyr_ext_modem_uart_transport_read_char(ext_modem_uart_transport, timeout, &c);
        if(!success) {
            return false;
        }

        if( ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) ) {
            // Retrieve half-byte value from character
            uint8_t hb = 0;
            if (c >= '0' && c <= '9' ) {
                hb = 0x0 + (c - '0');
            } else if (c >= 'a' && c <= 'f') {
                hb = 0xA + (c - 'a');
            } else /* (c >= 'A' && c <= 'F') */ {
                hb = 0xA + (c - 'A');
            }

            // Ignore character if buffer set to NULL
            if(wait_for_msb) {
                if(buf != NULL) {
                    buf[read_sz] = (hb << 4u);
                }
            } else {
                if(buf != NULL) {
                    buf[read_sz] |= hb;
                }
                read_sz++;
            }
            wait_for_msb = !wait_for_msb;
        } else if(c == '\n') {
            // End of frame
            *done = true;
            break;
        }
    } while(true);

    // Update size
    *sz = read_sz;

    return true;
}

bool blecon_zephyr_ext_modem_uart_transport_read_char(struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport, uint32_t timeout_ms, char* c) {
    // Check error flag
    if( ext_modem_uart_transport->rx_error ) {
        // Clear error flag
        uart_irq_rx_disable(ext_modem_uart_transport->uart_device);
        ext_modem_uart_transport->rx_error = false;
        uart_irq_rx_enable(ext_modem_uart_transport->uart_device);
        return false;
    }

    while( ring_buf_is_empty(&ext_modem_uart_transport->rx_fifo) ) {
        // Wait for data
        if( k_sem_take(&ext_modem_uart_transport->rx_sem, K_MSEC(timeout_ms)) < 0 ) {
            return false;
        }
    }

    uint32_t sz = ring_buf_get(&ext_modem_uart_transport->rx_fifo, c, 1);
    if( sz == 0 ) {
        return false;
    }

    return true;
}

bool blecon_zephyr_ext_modem_uart_transport_write_bytes(struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport, const uint8_t* data, size_t sz) {
    for(size_t write_pos = 0; write_pos < sz; write_pos++) {
        for(size_t half_byte_pos = 0; half_byte_pos <= 1; ) { 
            static const uint8_t digit2char[] = "0123456789ABCDEF";
            char c = digit2char[(data[write_pos] >> (4u * (1u - half_byte_pos))) & 0xF];
            
            bool success = blecon_zephyr_ext_modem_uart_transport_write_char(ext_modem_uart_transport, c);
            if(!success) {
                return false;
            }

            half_byte_pos++;
        }
    }

    return true;
}

bool blecon_zephyr_ext_modem_uart_transport_write_done(struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport) {
    // Send line termination
    bool success = blecon_zephyr_ext_modem_uart_transport_write_char(ext_modem_uart_transport, '\n');
    if(!success) {
        return false;
    }

    return true;
}

bool blecon_zephyr_ext_modem_uart_transport_write_char(struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport, char c) {
    // Use blocking API
    uart_poll_out(ext_modem_uart_transport->uart_device, c);
    return true;
}

void blecon_zephyr_ext_modem_uart_transport_rx_event(struct blecon_event_t* event, void* user_data) {
    struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport = (struct blecon_zephyr_ext_modem_uart_transport_t*) user_data;

    // Check that it's an event
    uint8_t buf[16] = {0};
    size_t read_sz = 1;
    bool rx_done = false;
    bool success = blecon_zephyr_ext_modem_uart_transport_read_bytes(ext_modem_uart_transport, buf, &read_sz, 0, &rx_done);
    if(!success) {
        goto clear;
    }

    if((read_sz == 1) && (buf[0] & 0x02)) {
        blecon_ext_modem_transport_signal(&ext_modem_uart_transport->ext_modem_transport);
    }

    // Expect frame to be done
    do {
        size_t read_sz = sizeof(buf);
        bool success = blecon_zephyr_ext_modem_uart_transport_read_bytes(ext_modem_uart_transport, buf, &read_sz, UART_TRANSPORT_DEFAULT_NEXT_CHARS_TIMEOUT_MS, &rx_done);
        if(!success) {
            return;
        }
    } while(!rx_done);
clear:
}

void blecon_zephyr_ext_modem_uart_transport_interrupt_handler(const struct device* dev, void* user_data) {
    struct blecon_zephyr_ext_modem_uart_transport_t* ext_modem_uart_transport = (struct blecon_zephyr_ext_modem_uart_transport_t*) user_data;
    bool signal_event = false;
    while( uart_irq_update(dev) && uart_irq_is_pending(dev) ) {
        if( uart_irq_rx_ready(dev) ) {
            while(true) {
                // Allocate space in the ring buffer
                uint8_t* rx_buf = NULL;
                uint32_t rx_sz = ring_buf_put_claim(&ext_modem_uart_transport->rx_fifo, &rx_buf, sizeof(ext_modem_uart_transport->rx_fifo_buf));

                if( rx_sz == 0 ) {
                    // No space left in ring buffer - discard bytes and set error flag
                    ring_buf_put_finish(&ext_modem_uart_transport->rx_fifo, 0);

                    uint8_t buf[16] = {0};
                    while( uart_fifo_read(dev, buf, sizeof(buf)) > 0 );

                    // Set error flag
                    ext_modem_uart_transport->rx_error = true;
                    signal_event = true;
                    break;
                }
                
                // Read UART FIFO
                int ret = uart_fifo_read(dev, rx_buf, rx_sz);
                blecon_assert(ret >= 0);

                // Adjust written size in ring buffer
                rx_sz = ret;
                ring_buf_put_finish(&ext_modem_uart_transport->rx_fifo, rx_sz);
                
                if( rx_sz == 0 ) {
                    // Done - got all data from FIFO
                    break;
                }

                signal_event = true;
            }
        }
    }

    if(!signal_event) {
        // Nothing happened
        return;
    }

    // Raise RX event
    blecon_event_signal(ext_modem_uart_transport->event);

    // Signal using semaphore
    k_sem_give(&ext_modem_uart_transport->rx_sem);
}