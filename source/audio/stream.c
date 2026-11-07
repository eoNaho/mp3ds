#include "stream.h"
#include <string.h>

static int same_track(const Track *a, const Track *b) {
    return a->size == b->size && a->mtime == b->mtime && !strcmp(a->path, b->path);
}
static Decoder *open_track(const Track *t) {
    return decoder_open_cached(t->path, &t->mp3, t->size, t->mtime);
}
void stream_close(AudioStream *s) {
    decoder_close(s->current); decoder_close(s->next);
    memset(s, 0, sizeof *s); s->next_id = -1;
}
int stream_start(AudioStream *s, const Track *t, uint32_t seek_ms) {
    Decoder *ready = NULL;
    if (s->next && same_track(&s->next_track, t)) { ready = s->next; s->next = NULL; }
    stream_close(s);
    s->current = ready ? ready : open_track(t);
    if (!s->current) return -1;
    if (seek_ms) decoder_seek_ms(s->current, seek_ms);
    return 0;
}
void stream_prepare(AudioStream *s, const Track *t, int id) {
    if (t && s->next && same_track(&s->next_track, t)) { s->next_id = id; return; }
    decoder_close(s->next); s->next = NULL; s->next_id = -1;
    if (t) { s->next = open_track(t); s->next_track = *t; s->next_id = id; }
}
int stream_read(AudioStream *s, int16_t *pcm, int frames, int *started_id) {
    *started_id = -1;
    if (!s->current) return 0;
    int n = decoder_read(s->current, pcm, frames);
    if (n || !s->next || decoder_sample_rate(s->current) != decoder_sample_rate(s->next)) return n;
    decoder_close(s->current); s->current = s->next; s->next = NULL;
    *started_id = s->next_id; s->next_id = -1;
    return decoder_read(s->current, pcm, frames);
}
