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

/* API DSP minima para testar o player com um consumidor controlado pelo teste. */
#include <time.h>
#include <errno.h>
typedef uint32_t u32;
typedef uint16_t u16;
typedef struct {
    void *data_vaddr;
    u32 nsamples;
    bool looping;
    volatile int status;
    u16 sequence_id;
} ndspWaveBuf;
enum { NDSP_WBUF_DONE, NDSP_WBUF_QUEUED, NDSP_WBUF_PLAYING };
#define NDSP_OUTPUT_STEREO 0
#define NDSP_INTERP_LINEAR 0
#define NDSP_FORMAT_STEREO_PCM16 0
#define R_FAILED(rc) ((rc) < 0)
static inline void LightEvent_WaitTimeout(LightEvent *e, int64_t ns) {
    struct timespec until; clock_gettime(CLOCK_REALTIME, &until);
    until.tv_nsec += ns; until.tv_sec += until.tv_nsec / 1000000000; until.tv_nsec %= 1000000000;
    pthread_mutex_lock(&e->mutex);
    while (!e->signaled) if (pthread_cond_timedwait(&e->cv, &e->mutex, &until) == ETIMEDOUT) break;
    e->signaled = 0; pthread_mutex_unlock(&e->mutex);
}
int ndspInit(void);
void ndspExit(void);
void ndspSetCallback(void (*cb)(void *), void *arg);
void ndspChnWaveBufAdd(int ch, ndspWaveBuf *b);
void ndspChnWaveBufClear(int ch);
u16 ndspChnGetWaveBufSeq(int ch);
u32 ndspChnGetSamplePos(int ch);
void ndspChnSetPaused(int ch, bool paused);
static inline void *linearAlloc(size_t bytes) { return malloc(bytes); }
static inline void linearFree(void *p) { free(p); }
static inline void ndspSetOutputMode(int mode) { (void)mode; }
static inline void ndspChnReset(int ch) { (void)ch; }
static inline void ndspChnSetInterp(int ch, int mode) { (void)ch; (void)mode; }
static inline void ndspChnSetFormat(int ch, int mode) { (void)ch; (void)mode; }
static inline void ndspChnSetRate(int ch, float rate) { (void)ch; (void)rate; }
static inline void ndspChnSetMix(int ch, float *mix) { (void)ch; (void)mix; }
static inline void DSP_FlushDataCache(void *p, u32 n) { (void)p; (void)n; }
static inline void ndspChnIirBiquadSetEnable(int ch, bool on) { (void)ch; (void)on; }
static inline void ndspChnIirBiquadSetParamsPeakingEqualizer(int ch, float f, float q, float g) { (void)ch; (void)f; (void)q; (void)g; }
static inline int APT_SetAppCpuTimeLimit(int percent) { (void)percent; return 0; }
