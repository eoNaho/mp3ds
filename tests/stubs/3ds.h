#pragma once
/* Adaptador POSIX somente para testar o worker de persistencia no PC. */
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
typedef int32_t s32;
typedef pthread_mutex_t LightLock;
typedef struct { pthread_mutex_t mutex; pthread_cond_t cv; int signaled; } LightEvent;
typedef struct TestThread { pthread_t id; void (*entry)(void *); void *arg; } *Thread;
#define RESET_ONESHOT 0
#define CUR_THREAD_HANDLE 0
#define U64_MAX UINT64_MAX
static inline void LightLock_Init(LightLock *l) { pthread_mutex_init(l, NULL); }
static inline void LightLock_Lock(LightLock *l) { pthread_mutex_lock(l); }
static inline void LightLock_Unlock(LightLock *l) { pthread_mutex_unlock(l); }
static inline void LightEvent_Init(LightEvent *e, int mode) {
    (void)mode; pthread_mutex_init(&e->mutex, NULL); pthread_cond_init(&e->cv, NULL); e->signaled = 0;
}
static inline void LightEvent_Signal(LightEvent *e) {
    pthread_mutex_lock(&e->mutex); e->signaled = 1; pthread_cond_signal(&e->cv); pthread_mutex_unlock(&e->mutex);
}
static inline void LightEvent_Wait(LightEvent *e) {
    pthread_mutex_lock(&e->mutex);
    while (!e->signaled) pthread_cond_wait(&e->cv, &e->mutex);
    e->signaled = 0; pthread_mutex_unlock(&e->mutex);
}
static inline void svcGetThreadPriority(s32 *prio, int handle) { (void)handle; *prio = 0x30; }
static inline void *test_thread_entry(void *p) { Thread t = p; t->entry(t->arg); return NULL; }
static inline Thread threadCreate(void (*entry)(void *), void *arg, size_t stack, s32 prio, int cpu, bool detached) {
    (void)stack; (void)prio; (void)cpu; (void)detached;
    Thread t = malloc(sizeof *t); if (!t) return NULL;
    t->entry = entry; t->arg = arg;
    if (pthread_create(&t->id, NULL, test_thread_entry, t)) { free(t); return NULL; }
    return t;
}
static inline void threadJoin(Thread t, uint64_t timeout) { (void)timeout; pthread_join(t->id, NULL); }
static inline void threadFree(Thread t) { free(t); }
