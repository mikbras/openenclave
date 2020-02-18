#include <errno.h>
#include <string.h>
#include "posix_cond.h"
#include "posix_mutex.h"
#include "posix_ocalls.h"

int posix_cond_init(posix_cond_t* c)
{
    if (!c)
        return EINVAL;

    memset(c, 0, sizeof(posix_cond_t));
    c->lock = 0;

    return 0;
}

int posix_cond_destroy(posix_cond_t* c)
{
    if (!c)
        return EINVAL;

    posix_spin_lock(&c->lock);

    /* Fail if queue is not empty */
    if (c->queue.front)
    {
        posix_spin_unlock(&c->lock);
        return EBUSY;
    }

    posix_spin_unlock(&c->lock);

    return 0;
}

int posix_cond_wait(posix_cond_t* c, posix_mutex_t* mutex)
{
    posix_thread_t* self = posix_self();

    if (!c || !mutex)
        return EINVAL;

    posix_spin_lock(&c->lock);
    {
        posix_thread_t* waiter = NULL;

        /* Add the self thread to the end of the wait queue */
        posix_thread_queue_push_back(&c->queue, self);

        /* Unlock this mutex and get the waiter at the front of the queue */
        if (__posix_mutex_unlock(mutex, &waiter) != 0)
        {
            posix_spin_unlock(&c->lock);
            return EBUSY;
        }

        for (;;)
        {
            posix_spin_unlock(&c->lock);
            {
                if (waiter)
                {
                    posix_wake_wait_ocall(waiter->host_uaddr, self->host_uaddr);
                    waiter = NULL;
                }
                else
                {
                    posix_wait_ocall(self->host_uaddr);
                }
            }
            posix_spin_lock(&c->lock);

            /* If self is no longer in the queue, then it was selected */
            if (!posix_thread_queue_contains(&c->queue, self))
                break;
        }
    }
    posix_spin_unlock(&c->lock);
    posix_mutex_lock(mutex);

    return 0;
}

int posix_cond_signal(posix_cond_t* c)
{
    posix_thread_t* waiter;

    if (!c)
        return EINVAL;

    posix_spin_lock(&c->lock);
    waiter = posix_thread_queue_pop_front(&c->queue);
    posix_spin_unlock(&c->lock);

    if (!waiter)
        return 0;

    posix_wake_ocall(waiter->host_uaddr);
    return 0;
}

int posix_cond_broadcast(posix_cond_t* c)
{
    posix_thread_queue_t waiters = {NULL, NULL};

    if (!c)
        return EINVAL;

    posix_spin_lock(&c->lock);
    {
        posix_thread_t* p;

        while ((p = posix_thread_queue_pop_front(&c->queue)))
            posix_thread_queue_push_back(&waiters, p);
    }
    posix_spin_unlock(&c->lock);

    posix_thread_t* p_next = NULL;
    for (posix_thread_t* p = waiters.front; p; p = p_next)
    {
        // p could wake up and immediately use a synchronization
        // primitive that could modify the next field.
        // Therefore fetch the next thread before waking up p.
        p_next = p->next;
        posix_wake_ocall(p->host_uaddr);
    }

    return 0;
}
