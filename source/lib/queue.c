#include "queue.h"
#include <stdlib.h>
#include <string.h>

static unsigned rnd(Queue *q) { q->rng ^= q->rng << 13; q->rng ^= q->rng >> 17; q->rng ^= q->rng << 5; return q->rng; }

void queue_init(Queue *q, unsigned seed) { memset(q, 0, sizeof *q); q->rng = seed ? seed : 2463534242u; }

void queue_free(Queue *q) { free(q->items); free(q->order); q->items = q->order = NULL; q->count = q->pos = 0; }

static void identity(Queue *q) { for (int i = 0; i < q->count; i++) q->order[i] = i; }

static void shuffle_keep(Queue *q, int keep_idx) {
    identity(q);
    for (int i = q->count - 1; i > 0; i--) { int j = (int)(rnd(q) % (unsigned)(i + 1)); int t = q->order[i]; q->order[i] = q->order[j]; q->order[j] = t; }
    for (int i = 0; i < q->count; i++) if (q->order[i] == keep_idx) { q->order[i] = q->order[0]; q->order[0] = keep_idx; break; }
    q->pos = 0;
}

int queue_set(Queue *q, const int *items, int count, int start) {
    int *it = malloc(sizeof(int) * (count > 0 ? count : 1)), *or_ = malloc(sizeof(int) * (count > 0 ? count : 1));
    if (!it || !or_) { free(it); free(or_); return -1; }
    free(q->items); free(q->order);
    memcpy(it, items, sizeof(int) * count);
    q->items = it; q->order = or_; q->count = count;
    if (start < 0 || start >= count) start = 0;
    if (q->shuffle) shuffle_keep(q, start); else { identity(q); q->pos = start; }
    return 0;
}

int queue_current(const Queue *q) { return (q->count > 0) ? q->items[q->order[q->pos]] : -1; }

int queue_next(Queue *q, int manual) {
    if (q->count == 0) return -1;
    if (q->repeat == REPEAT_ONE && !manual) return queue_current(q);
    if (q->pos + 1 < q->count) q->pos++;
    else if (q->repeat == REPEAT_ALL || (manual && q->repeat == REPEAT_ONE)) q->pos = 0;   /* 'proxima' manual no fim volta ao inicio */
    else return -1;
    return queue_current(q);
}

int queue_prev(Queue *q) {
    if (q->count == 0) return -1;
    if (q->pos > 0) q->pos--;
    else if (q->repeat != REPEAT_OFF) q->pos = q->count - 1;
    return queue_current(q);
}

void queue_set_shuffle(Queue *q, int on) {
    on = !!on;
    if (on == q->shuffle) return;
    q->shuffle = on;
    if (q->count == 0) return;
    int cur = q->order[q->pos];
    if (on) shuffle_keep(q, cur); else { identity(q); q->pos = cur; }
}
