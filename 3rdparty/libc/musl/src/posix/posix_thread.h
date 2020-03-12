#ifndef _POSIX_THREAD_H
#define _POSIX_THREAD_H

#include <stdint.h>
#include <setjmp.h>
#include <unistd.h>
#include <stdbool.h>
#include "posix_ocall_structs.h"
#include "posix_shared_block.h"
#include "posix_spinlock.h"
#include "posix_common.h"
#include "posix_thread_queue.h"

typedef struct _posix_thread posix_thread_t;
typedef struct _posix_mutex posix_mutex_t;

struct posix_robust_list_head
{
    volatile void* volatile head;
};

#define POSIX_THREAD_STATE_STARTED 0xAAAABBBB

struct _posix_thread
{
    /* Should contain MAGIC */
    uint32_t magic;

    posix_thread_t* next;
    posix_thread_t* prev;

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
    volatile pid_t* ctid; /* host address */

    /* Used to jump from posix_exit() back to posix_run_thread_ecall() */
    jmp_buf jmpbuf;

    /* Address of the host thread's shared page */
    struct posix_shared_block* shared_block;

    /* TID passed to posix_run_thread_ecall() */
    int tid;

    /* Robust list support */
    struct posix_robust_list_head* robust_list_head;
    size_t robust_list_len;

    /* Spin here until thread is actually created */
    posix_spinlock_t create_lock;

    /* POSIX_THREAD_STATE_STARTED or zero */
    uint32_t state;
};

posix_thread_t* posix_self(void);

posix_shared_block_t* posix_shared_block(void);

int posix_set_tid_address(int* tidptr);

int posix_set_thread_area(void* p);

int posix_clone(
    int (*fn)(void *),
    void* child_stack,
    int flags,
    void* arg,
    ...);

int posix_gettid(void);

int posix_getpid(void);

long posix_get_robust_list(
    int pid,
    struct posix_robust_list_head** head_ptr,
    size_t* len_ptr);

long posix_set_robust_list(struct posix_robust_list_head* head, size_t len);

long posix_tkill_syscall(int tid, int sig);

void posix_noop(void);

void posix_exit(int status);

/* ATTN: use a more elegant locking solution */
void posix_unblock_creator_thread(void);

int posix_init_main_thread(posix_shared_block_t* shared_block, int tid);

#endif /* _POSIX_THREAD_H */
