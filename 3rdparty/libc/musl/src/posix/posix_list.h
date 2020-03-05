// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef _POSIX_LIST_H
#define _POSIX_LIST_H

#include "posix_common.h"

typedef struct _posix_list_node posix_list_node_t;

typedef struct _posix_list posix_list_t;

struct _posix_list
{
    posix_list_node_t* head;
    posix_list_node_t* tail;
};

struct _posix_list_node
{
    posix_list_node_t* prev;
    posix_list_node_t* next;
};

POSIX_INLINE void posix_list_remove(
    posix_list_t* list,
    posix_list_node_t* node)
{
    if (list && node)
    {
        if (node->prev)
            node->prev->next = node->next;
        else
            list->head = node->next;

        if (node->next)
            node->next->prev = node->prev;
        else
            list->tail = node->prev;
    }
}

POSIX_INLINE void posix_list_push_front(
    posix_list_t* list,
    posix_list_node_t* node)
{
    if (list && node)
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
    }
}

POSIX_INLINE void posix_list_push_back(
    posix_list_t* list,
    posix_list_node_t* node)
{
    if (list && node)
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
    }
}

POSIX_INLINE posix_list_node_t* posix_list_pop_front(posix_list_t* list)
{
    if (list)
    {
        posix_list_node_t* node = list->head;

        if (node)
            posix_list_remove(list, node);

        return node;
    }

    return NULL;
}

POSIX_INLINE posix_list_node_t* posix_list_pop_back(posix_list_t* list)
{
    if (list)
    {
        posix_list_node_t* node = list->tail;

        if (node)
            posix_list_remove(list, node);

        return node;
    }

    return NULL;
}

POSIX_INLINE void posix_list_free(posix_list_t* list, void (*free)(void*))
{
    if (list && free)
    {
        for (posix_list_node_t* p = list->head; p;)
        {
            posix_list_node_t* next = p->next;
            (*free)(p);
            p = next;
        }

        list->head = NULL;
        list->tail = NULL;
    }
}

POSIX_INLINE posix_list_node_t* posix_list_find(
    posix_list_t* list,
    bool (*match)(posix_list_node_t*))
{
    if (list && match)
    {
        for (posix_list_node_t* p = list->head; p;)
        {
            if ((*match)(p))
                return p;
        }
    }

    return NULL;
}

#endif /* _POSIX_LIST_H */
