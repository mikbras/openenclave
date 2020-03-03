#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

#define __NEED_time_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>

#include <fcntl.h>

#ifndef FUTEX_MAP
volatile int* posix_futex_map(volatile int* lock);
#define FUTEX_MAP(LOCK_ADDR) posix_futex_map(&LOCK_ADDR)[0]
#define FUTEX_MAP_PTR(LOCK_ADDR) posix_futex_map(LOCK_ADDR)
#endif

#define SEM_FAILED ((sem_t *)0)

typedef struct {
	volatile int zzz__val[4*sizeof(long)/sizeof(int)];
} sem_t;

int    sem_close(sem_t *);
int    sem_destroy(sem_t *);
int    sem_getvalue(sem_t *__restrict, int *__restrict);
int    sem_init(sem_t *, int, unsigned);
sem_t *sem_open(const char *, int, ...);
int    sem_post(sem_t *);
int    sem_timedwait(sem_t *__restrict, const struct timespec *__restrict);
int    sem_trywait(sem_t *);
int    sem_unlink(const char *);
int    sem_wait(sem_t *);

#ifdef __cplusplus
}
#endif
#endif
