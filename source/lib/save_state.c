#include "save_state.h"
#include "session.h"
#include "resume.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

SaveState *save_state_capture(const Library *lib, const Queue *q,
                             const char *path, uint32_t ms, int index) {
    SaveState *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->lib.tracks = malloc(sizeof(Track) * (lib->count ? lib->count : 1));
    if (!s->lib.tracks) { save_state_free(s); return NULL; }
    s->lib.count = s->lib.cap = lib->count;
    if (lib->count) memcpy(s->lib.tracks, lib->tracks, sizeof(Track) * lib->count);
    if (q->count) {
        if (queue_set(&s->queue, q->items, q->count, q->pos) != 0) { save_state_free(s); return NULL; }
        memcpy(s->queue.order, q->order, sizeof(int) * q->count);
    }
    s->queue.shuffle = q->shuffle; s->queue.repeat = q->repeat;
    u_copy(s->resume_path, sizeof s->resume_path, path ? path : "");
    s->resume_ms = ms; s->save_index = index;
    return s;
}

void save_state_free(SaveState *s) {
    if (!s) return;
    library_free(&s->lib); queue_free(&s->queue); free(s);
}

int save_state_write(const SaveState *s, const char *session, const char *resume, const char *index) {
    int rc = session_save(session, &s->lib, &s->queue);
    if (resume_save(resume, s->resume_path, s->resume_ms) != 0) rc = -1;
    if (s->save_index && library_save_index(&s->lib, index) != 0) rc = -1;
    return rc;
}
