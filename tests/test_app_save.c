#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "player.h"
#include "persistence.h"
static int error_pending = 1, submissions, closed, positions;
static PlayerState state = PS_PLAYING;
int persistence_init(void) { return 0; }
int persistence_busy(void) { return 0; }
void persistence_exit(void) { closed = 1; }
int persistence_take_error(void) { return 0; }
int persistence_take_error_detail(int *parts, int *code) {
    if (!error_pending) return 0;
    error_pending = 0; *parts = SAVE_SESSION_ERROR; *code = ENOSPC; return 1;
}
int persistence_submit_parts(const Library *l, const Queue *q, const char *path, uint32_t ms, int session, int index) {
    (void)l; (void)q; (void)path; (void)ms; (void)session; (void)index;
    assert(!closed); submissions++; return 0;
}
PlayerState player_state(void) { return state; }
uint32_t player_pos_ms(void) { assert(state != PS_STOPPED); positions++; return 1234; }
uint32_t player_duration_ms(void) { return 10000; }
void player_play_track(const Track *t, uint32_t seek) { (void)t; (void)seek; }
void player_pause(int on) { (void)on; }
void player_seek(uint32_t ms) { (void)ms; }
void player_stop(void) { state = PS_STOPPED; }
void player_prepare_next(const Track *t, int id) { (void)t; (void)id; }
int player_take_finished(void) { return 0; }
int player_take_started(void) { return -1; }
int main(void) {
    App *a = calloc(1, sizeof *a); assert(a);
    a->lib.tracks = calloc(1, sizeof(Track)); assert(a->lib.tracks);
    a->lib.count = a->lib.cap = 1; a->cur = 0;
    strcpy(a->lib.tracks[0].path, "song.mp3");
    app_tick(a);
    assert(!submissions && a->save_retry_frames == 150);
    assert(strstr(a->toast, "session.txt") && strstr(a->toast, "28"));
    for (int i = 0; i < 149; i++) { app_tick(a); app_save_resume(a); }
    assert(!submissions); app_tick(a); assert(submissions == 1);
    a->save_retry_frames = 150; app_capture_resume(a);
    int before = positions; player_stop(); app_exit(a);
    assert(positions == before && closed && submissions == 2);
    assert(a->resume_ms == 1234 && !strcmp(a->resume_path, "song.mp3"));
    free(a);
    puts("ok app: falha identificada, retry limitado e saida apos encerrar DSP");
}
