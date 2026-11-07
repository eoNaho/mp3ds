#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "persistence.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate = PTHREAD_COND_INITIALIZER;
static int entered, release_writer, calls;
static uint32_t last_ms;
static int last_index;

int __wrap_save_state_write(const SaveState *s, const char *session, const char *resume, const char *index) {
    (void)session; (void)resume; (void)index;
    pthread_mutex_lock(&mutex);
    if (!calls) {
        entered = 1; pthread_cond_signal(&gate);
        while (!release_writer) pthread_cond_wait(&gate, &mutex);
    }
    assert(s->lib.count == 1 && !strcmp(s->lib.tracks[0].path, "faixa.mp3"));
    assert(queue_current(&s->queue) == 0);
    last_ms = s->resume_ms; last_index = s->save_index;
    int rc = calls++ == 0 ? -1 : 0;
    pthread_mutex_unlock(&mutex);
    return rc;
}

int main(void) {
    alarm(15); /* uma espera travada deve falhar o teste, nao o CI */
    Track track = {0}; strcpy(track.path, "faixa.mp3");
    Library lib = {.tracks = &track, .count = 1};
    Queue q; queue_init(&q, 1); int id = 0;
    assert(queue_set(&q, &id, 1, 0) == 0);
    assert(persistence_init() == 0);
    assert(persistence_submit(&lib, &q, track.path, 1, 0) == 0);
    pthread_mutex_lock(&mutex);
    while (!entered) pthread_cond_wait(&gate, &mutex);
    pthread_mutex_unlock(&mutex);
    assert(persistence_busy());
    /* O SD permanece bloqueado enquanto a UI envia novos estados. */
    for (uint32_t i = 2; i <= 100; i++)
        assert(persistence_submit(&lib, &q, track.path, i, i == 2) == 0);
    memset(&track, 0, sizeof track); queue_free(&q);
    pthread_mutex_lock(&mutex);
    release_writer = 1; pthread_cond_signal(&gate);
    pthread_mutex_unlock(&mutex);
    persistence_exit();
    assert(calls == 2 && last_ms == 100 && last_index == 1);
    assert(!persistence_busy());
    assert(persistence_take_error() == 1 && persistence_take_error() == 0);
    puts("ok persistence: SD lento, ultimo estado, erros e encerramento");
    return 0;
}
