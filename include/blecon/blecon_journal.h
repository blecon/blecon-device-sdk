/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

typedef uint32_t blecon_journal_event_type_t;

struct blecon_journal_t {
    uint8_t* array;
    size_t array_sz;
    const size_t* event_type_size;
    size_t event_type_size_count;

    uint8_t* write_pos;
    const uint8_t* read_pos;
    uint32_t read_event_id;
};

struct blecon_journal_iterator_t {
    struct blecon_journal_t* journal;
    const uint8_t* read_pos;
    uint32_t read_event_id;
    bool valid;
};

void blecon_journal_init(struct blecon_journal_t* journal,
                            uint8_t* array, size_t array_sz,
                            const size_t* event_type_size, size_t event_type_size_count
                        );

void blecon_journal_push(struct blecon_journal_t* journal, uint32_t timestamp, blecon_journal_event_type_t event_type, const void* event);

bool blecon_journal_is_empty(const struct blecon_journal_t* journal);

struct blecon_journal_iterator_t blecon_journal_begin(struct blecon_journal_t* journal);

struct blecon_journal_iterator_t blecon_journal_next(const struct blecon_journal_iterator_t* journal_iterator);

void blecon_journal_get_metadata(const struct blecon_journal_iterator_t* journal_iterator, 
    uint32_t* id, uint32_t* timestamp,
    blecon_journal_event_type_t* event_type, size_t* event_size);
void blecon_journal_get_event(const struct blecon_journal_iterator_t* journal_iterator, void* event);

void blecon_journal_erase_until(struct blecon_journal_iterator_t* journal_iterator);

// Returns size in bytes
size_t blecon_journal_size(struct blecon_journal_t* journal);

#ifdef __cplusplus
}
#endif
