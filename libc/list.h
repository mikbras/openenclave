// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef _LIST_H
#define _LIST_H

#include <openenclave/bits/defs.h>
#include <openenclave/bits/types.h>

typedef struct list_node list_node_t;

typedef struct list list_t;

struct list
{
    list_node_t* head;
    list_node_t* tail;
    size_t size;
};

struct list_node
{
    list_node_t* prev;
    list_node_t* next;
};

OE_INLINE void list_remove(list_t* list, list_node_t* node)
{
    if (node->prev)
        node->prev->next = node->next;
    else
        list->head = node->next;

    if (node->next)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;

    list->size--;
}

OE_INLINE void list_prepend(list_t* list, list_node_t* node)
{
    if (list->head)
    {
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    else
    {
        node->next = NULL;
        node->prev = NULL;
        list->head = node;
        list->tail = node;
    }

    list->size++;
}

OE_INLINE void list_append(list_t* list, list_node_t* node)
{
    if (list->tail)
    {
        node->next = NULL;
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }
    else
    {
        node->next = NULL;
        node->prev = NULL;
        list->head = node;
        list->tail = node;
    }

    list->size++;
}

#endif /* _LIST_H */
