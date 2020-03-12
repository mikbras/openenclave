#include <errno.h>
#include <string.h>
#include <assert.h>
#include "posix_cond.h"
#include "posix_mutex.h"
#include "posix_ocalls.h"
#include "posix_ocall_structs.h"
#include "posix_io.h"
#include "posix_signal.h"
#include "posix_panic.h"
#include "posix_assume.h"
/* */
#include "posix_warnings.h"

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

int posix_cond_timedwait(
    posix_cond_t* c,
    posix_mutex_t* mutex,
    const struct posix_timespec* timeout)
{
    posix_thread_t* self = posix_self();
    int ret = 0;

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
                    int retval;

                    if (POSIX_OCALL(posix_thread_wake_wait_ocall(
                        &retval,
                        &waiter->shared_block->futex,
                        &self->shared_block->futex,
                        (struct posix_timespec*)timeout), 0x89ab8e9a) != OE_OK)
                    {
                        POSIX_PANIC;
                    }

                    POSIX_ASSUME(retval <= 0);
                    ret = -retval;

                    waiter = NULL;
                }
                else
                {
                    int retval;

                    if (POSIX_OCALL(posix_thread_wait_ocall(
                        &retval,
                        &self->shared_block->futex,
                        (struct posix_timespec*)timeout), 0xf19a9a78) != OE_OK)
                    {
                        POSIX_PANIC;
                    }

                    POSIX_ASSUME(retval <= 0);
                    ret = -retval;
                }
            }
            posix_spin_lock(&c->lock);

            /* If self is no longer in the queue, then it was selected */
            if (!posix_thread_queue_contains(&c->queue, self))
                break;

            if (ret != 0)
                break;
        }
    }
    posix_spin_unlock(&c->lock);
    posix_mutex_lock(mutex);

    return ret;
}

int posix_cond_wait(posix_cond_t* c, posix_mutex_t* mutex)
{
    return posix_cond_timedwait(c, mutex, NULL);
}

int posix_cond_signal(posix_cond_t* c)
{
    posix_thread_t* waiter;
    int retval;

    if (!c)
        return EINVAL;

    posix_spin_lock(&c->lock);
    waiter = posix_thread_queue_pop_front(&c->queue);
    posix_spin_unlock(&c->lock);

    if (!waiter)
        return 0;

    if (POSIX_OCALL(posix_thread_wake_ocall(
        &retval, &waiter->shared_block->futex), 0x6f030c81) != OE_OK)
    {
        POSIX_PANIC;
    }

    if (retval != 0)
        POSIX_PANIC_MSG("wake failed: retval=%d", retval);

    return 0;
}

int posix_cond_broadcast(posix_cond_t* c, size_t n)
{
    posix_thread_queue_t waiters = {NULL, NULL};

    if (!c)
        return EINVAL;

    posix_spin_lock(&c->lock);
    {
        posix_thread_t* p;
        size_t i = 0;

        /* Select at most n waiters to be woken up */
        while (i < n && (p = posix_thread_queue_pop_front(&c->queue)))
        {
            posix_thread_queue_push_back(&waiters, p);
            i++;
        }
    }
    posix_spin_unlock(&c->lock);

    posix_thread_t* next = NULL;

    for (posix_thread_t* p = waiters.front; p; p = next)
    {
        int retval;

        next = p->next;

        if (POSIX_OCALL(posix_thread_wake_ocall(
            &retval, &p->shared_block->futex), 0xc05170b3) != OE_OK)
        {
            POSIX_PANIC;
        }

        if (retval != 0)
            POSIX_PANIC_MSG("wake failed: retval=%d", retval);
    }

    return 0;
}

int posix_cond_requeue(
    posix_cond_t* c1,
    posix_cond_t* c2,
    size_t wake_count,
    size_t requeue_count)
{
    posix_thread_queue_t wakers = {NULL, NULL};
    posix_thread_queue_t requeues = {NULL, NULL};

    if (!c1 || !c2)
        return EINVAL;

    /* Form two queues: wakers and requeues */
    posix_spin_lock(&c1->lock);
    {
        /* Select threads to be awoken */
        for (size_t i = 0; i < wake_count; i++)
        {
            posix_thread_t* p;

            if (!(p = posix_thread_queue_pop_front(&c1->queue)))
                break;

            posix_thread_queue_push_back(&wakers, p);
        }

        /* Selector threads to be required */
        for (size_t i = 0; i < requeue_count; i++)
        {
            posix_thread_t* p;

            if (!(p = posix_thread_queue_pop_front(&c1->queue)))
                break;

            posix_thread_queue_push_back(&requeues, p);
        }
    }
    posix_spin_unlock(&c1->lock);

    /* Wake the threads in the wakers queue */
    {
        posix_thread_t* next = NULL;

        for (posix_thread_t* p = wakers.front; p; p = next)
        {
            int retval;

            next = p->next;

            if (POSIX_OCALL(posix_thread_wake_ocall(
                &retval, &p->shared_block->futex), 0x21f6b143) != OE_OK)
            {
                POSIX_PANIC;
            }

            if (retval != 0)
                POSIX_PANIC_MSG("wake failed: retval=%d", retval);
        }
    }

    /* Requeue the threads in the requeues queue */
    posix_spin_lock(&c2->lock);
    {
        posix_thread_t* next = NULL;

        for (posix_thread_t* p = requeues.front; p; p = next)
        {
            next = p->next;
            posix_thread_queue_push_back(&c2->queue, p);
        }
    }
    posix_spin_unlock(&c2->lock);

    return 0;
}
