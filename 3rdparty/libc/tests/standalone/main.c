#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static __thread int _val = 99;

static void* _thread(void* arg)
{
    printf("_val2=%d\n", _val);

    _val = 222;
    printf("_val2=%d\n", _val);

    for (size_t i = 0; i < 10; i++)
        printf("_thread(): %p\n", pthread_self());

    return NULL;
}

int main()
{
    printf("_val1=%d\n", _val);

    pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    pthread_key_t key;
    pthread_t t;

    pthread_key_create(&key, NULL);
    pthread_mutex_lock(&m);
    pthread_mutex_unlock(&m);

    _val = 111;
    printf("_val.t0=%d\n", _val);

    pthread_create(&t, NULL, _thread, NULL);
    pthread_create(&t, NULL, _thread, NULL);

    for (size_t i = 0; i < 10; i++)
    {
        printf("_thread(): %p\n", pthread_self());
    }

    pthread_join(t, NULL);

    return 0;
}

void _start(void)
{
    extern void posix_init_libc();
    posix_init_libc();

    exit(main());
}
