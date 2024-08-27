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


struct blecon_list_node_t;

struct blecon_list_t {
    struct blecon_list_node_t* head;
    struct blecon_list_node_t* tail;
};

struct blecon_list_node_t {
    struct blecon_list_node_t* next;
    struct blecon_list_node_t* prev;
};

void blecon_list_init(struct blecon_list_t* list);

void blecon_list_node_init(struct blecon_list_node_t* node);

void blecon_list_push_back(struct blecon_list_t* list, struct blecon_list_node_t* node);

struct blecon_list_node_t* blecon_list_pop_front(struct blecon_list_t* list);

void blecon_list_insert_after(struct blecon_list_t* list, struct blecon_list_node_t* node, struct blecon_list_node_t* previous_node);

void blecon_list_insert_before(struct blecon_list_t* list, struct blecon_list_node_t* node, struct blecon_list_node_t* next_node);

struct blecon_list_node_t* blecon_list_first(struct blecon_list_t* list);

struct blecon_list_node_t* blecon_list_last(struct blecon_list_t* list);

struct blecon_list_node_t* blecon_list_iterate_start(struct blecon_list_t* list);

struct blecon_list_node_t* blecon_list_iterate_next(struct blecon_list_node_t* node);

struct blecon_list_node_t* blecon_list_iterate_previous(struct blecon_list_node_t* node);

void blecon_list_remove(struct blecon_list_t* list, struct blecon_list_node_t* node);

void blecon_list_clear(struct blecon_list_t* list);

bool blecon_list_is_empty(struct blecon_list_t* list);

#ifdef __cplusplus
}
#endif
