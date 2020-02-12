static inline struct pthread *__pthread_self()
{
        extern struct pthread* posix_pthread_self(void);
        return posix_pthread_self();
}

#define TP_ADJ(p) (p)

#define MC_PC gregs[REG_RIP]
