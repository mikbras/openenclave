#ifndef _POSIX_THREAD_QUEUE_H
#define _POSIX_THREAD_QUEUE_H

#include "posix_common.h"

typedef struct _posix_thread posix_thread_t;

typedef struct _posix_thread_queue
{
    posix_thread_t* front;
    posix_thread_t* back;
} posix_thread_queue_t;

size_t posix_thread_queue_size(posix_thread_queue_t* queue);

void posix_thread_queue_push_back(
    posix_thread_queue_t* queue,
    posix_thread_t* thread);

posix_thread_t* posix_thread_queue_pop_front(posix_thread_queue_t* queue);

bool posix_thread_queue_contains(
    posix_thread_queue_t* queue,
    posix_thread_t* thread);

POSIX_INLINE bool posix_thread_queue_empty(const posix_thread_queue_t* queue)
{
    return queue->front ? false : true;
}

#endif /* _POSIX_THREAD_QUEUE_H */
