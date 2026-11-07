#include <3ds.h>
#include "persistence.h"
#include "config.h"

static Thread s_thread;
static LightLock s_lock;
static LightEvent s_wake;
static SaveState *s_pending;
static int s_run, s_busy, s_error;

static void worker(void *arg) {
    (void)arg;
    for (;;) {
        LightEvent_Wait(&s_wake);
        for (;;) {
            LightLock_Lock(&s_lock);
            SaveState *job = s_pending;
            s_pending = NULL;
            int running = s_run;
            s_busy = job != NULL;
            LightLock_Unlock(&s_lock);
            if (!job) { if (!running) return; break; }
            int rc = save_state_write(job, SESSION_PATH, RESUME_PATH, INDEX_PATH);
            save_state_free(job);
            LightLock_Lock(&s_lock);
            if (rc) s_error = 1;
            s_busy = 0;
            LightLock_Unlock(&s_lock);
        }
    }
}

int persistence_init(void) {
    LightLock_Init(&s_lock); LightEvent_Init(&s_wake, RESET_ONESHOT);
    s_run = 1; s_busy = s_error = 0; s_pending = NULL;
    s32 prio = 0x30; svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    s_thread = threadCreate(worker, NULL, 16 * 1024, prio + 1, -2, false);
    return s_thread ? 0 : -1;
}

int persistence_submit(const Library *lib, const Queue *q, const char *path, uint32_t ms, int index) {
    if (!s_thread) return -1;
    SaveState *job = save_state_capture(lib, q, path, ms, index);
    if (!job) return -1;
    LightLock_Lock(&s_lock);
    SaveState *old = s_pending;
    if (old && old->save_index) job->save_index = 1;
    s_pending = job;
    LightLock_Unlock(&s_lock);
    save_state_free(old);
    LightEvent_Signal(&s_wake);
    return 0;
}

int persistence_busy(void) {
    LightLock_Lock(&s_lock);
    int busy = s_busy || s_pending != NULL;
    LightLock_Unlock(&s_lock);
    return busy;
}

int persistence_take_error(void) {
    LightLock_Lock(&s_lock);
    int error = s_error; s_error = 0;
    LightLock_Unlock(&s_lock);
    return error;
}

void persistence_exit(void) {
    LightLock_Lock(&s_lock); s_run = 0; LightLock_Unlock(&s_lock);
    LightEvent_Signal(&s_wake);
    if (s_thread) { threadJoin(s_thread, U64_MAX); threadFree(s_thread); s_thread = NULL; }
}
