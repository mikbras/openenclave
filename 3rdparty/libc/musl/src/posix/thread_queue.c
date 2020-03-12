#include "posix_thread_queue.h"
#include "posix_thread.h"

size_t posix_thread_queue_size(posix_thread_queue_t* queue)
{
    size_t n = 0;

    for (posix_thread_t* p = queue->front; p; p = p->next)
        n++;

    return n;
}

void posix_thread_queue_push_back(
    posix_thread_queue_t* queue,
    posix_thread_t* thread)
{
    thread->next = NULL;

    if (queue->back)
        queue->back->next = thread;
    else
        queue->front = thread;

    queue->back = thread;
}

posix_thread_t* posix_thread_queue_pop_front(posix_thread_queue_t* queue)
{
    posix_thread_t* thread = queue->front;

    if (thread)
    {
        queue->front = queue->front->next;

        if (!queue->front)
            queue->back = NULL;
    }

    return thread;
}

bool posix_thread_queue_contains(
    posix_thread_queue_t* queue,
    posix_thread_t* thread)
{
    posix_thread_t* p;

    for (p = queue->front; p; p = p->next)
    {
        if (p == thread)
            return true;
    }

    return false;
}
