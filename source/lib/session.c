#include "config.h"
#include "session.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

int session_save(const char *file, const Library *lib, const Queue *q) {
    char tmp[TRACK_PATH_MAX + 8];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", file) >= (int)sizeof tmp) return -1;
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    int ok = fprintf(f, "mp3ds-session-1 %d %d %d\n", q->shuffle, q->repeat, q->pos) >= 0;
    for (int i = 0; i < q->count && ok; i++) {
        int id = queue_slot_id(q, i);
        if (id < 0 || id >= lib->count) { ok = 0; break; }
        ok = fprintf(f, "%s\n", lib->tracks[id].path) >= 0;
    }
    return u_commit_file(f, tmp, file, ok);
}

int session_load(const char *file, const Library *lib, Queue *q) {
    FILE *f = u_open_saved(file);
    if (!f) return -1;
    char line[TRACK_PATH_MAX + 8]; int shuffle, repeat, slot;
    if (!fgets(line, sizeof line, f) ||
        sscanf(line, "mp3ds-session-1 %d %d %d", &shuffle, &repeat, &slot) != 3 ||
        shuffle < 0 || shuffle > 1 || repeat < REPEAT_OFF || repeat > REPEAT_ONE || slot < 0) {
        fclose(f); return -1;
    }
    int *ids = NULL, n = 0, cap = 0, original = 0, start = -1, failed = 0;
    while (fgets(line, sizeof line, f)) {
        if (!strchr(line, '\n')) { failed = 1; break; }
        line[strcspn(line, "\r\n")] = 0;
        int id = library_find_path(lib, line);
        if (id >= 0) {
            if (n == cap) {
                if (cap > INT_MAX / (int)sizeof(int) / 2) { failed = 1; break; }
                int next = cap ? cap * 2 : 64;
                int *grown = realloc(ids, sizeof(int) * next);
                if (!grown) { failed = 1; break; }
                ids = grown; cap = next;
            }
            if (original >= slot && start < 0) start = n;
            ids[n++] = id;
        }
        original++;
    }
    if (ferror(f)) failed = 1;
    fclose(f);
    if (failed || !n) { free(ids); return -1; }
    /* A ordem salva ja inclui shuffle; nao embaralhar novamente. */
    int old_shuffle = q->shuffle; q->shuffle = 0;
    int rc = queue_set(q, ids, n, start < 0 ? n - 1 : start);
    q->shuffle = rc == 0 ? shuffle : old_shuffle;
    if (rc == 0) q->repeat = (RepeatMode)repeat;
    free(ids);
    return rc;
}
