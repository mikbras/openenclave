#ifndef _POSIX_THREAD_H
#define _POSIX_THREAD_H

#include <stdint.h>
#include <setjmp.h>
#include <unistd.h>

typedef struct _posix_thread_info posix_thread_info_t;

struct _posix_thread_info
{
    /* Should contain MAGIC */
    uint32_t magic;

    posix_thread_info_t* next;
    posix_thread_info_t* prev;

    /* Pointer to MUSL pthread structure */
    struct pthread* td;

    /* The fn parameter from posix_clone() */
    int (*fn)(void*);

    /* The arg parameter from posix_clone() */
    void* arg;

    /* The flags parameter from posix_clone() */
    int flags;

    /* The ptid parameter from posix_clone() */
    pid_t* ptid;

    /* The ctid parameter from posix_clone() (__thread_list_lock) */
    volatile pid_t* ctid;

    /* Used to jump from posix_exit() back to posix_run_thread_ecall() */
    jmp_buf jmpbuf;

    /* Address of the host thread's futex uaddr. */
    int* host_uaddr;
};

typedef struct _posix_thread_info_queue
{
    posix_thread_info_t* front;
    posix_thread_info_t* back;
} posix_thread_info_queue_t;

static __inline__ void posix_thread_info_queue_push_back(
    posix_thread_info_queue_t* queue,
    posix_thread_info_t* thread)
{
    thread->next = NULL;

    if (queue->back)
        queue->back->next = thread;
    else
        queue->front = thread;

    queue->back = thread;
}

static __inline__ posix_thread_info_t* posix_thread_info_queue_pop_front(
    posix_thread_info_queue_t* queue)
{
    posix_thread_info_t* thread = queue->front;

    if (thread)
    {
        queue->front = queue->front->next;

        if (!queue->front)
            queue->back = NULL;
    }

    return thread;
}

static __inline__ bool posix_thread_info_queue_contains(
    posix_thread_info_queue_t* queue,
    posix_thread_info_t* thread)
{
    posix_thread_info_t* p;

    for (p = queue->front; p; p = p->next)
    {
        if (p == thread)
            return true;
    }

    return false;
}

static __inline__ bool posix_thread_info_queue_empty(
    posix_thread_info_queue_t* queue)
{
    return queue->front ? false : true;
}

int posix_set_tid_address(int* tidptr);

int posix_set_thread_area(void* p);

int posix_clone(
    int (*fn)(void *),
    void* child_stack,
    int flags,
    void* arg,
    ...);

void posix_exit(int status);

int posix_gettid(void);

#endif /* _POSIX_THREAD_H */
