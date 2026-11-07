#pragma once
#include "library.h"
#include "queue.h"

typedef struct {
    Library lib;
    Queue queue;
    char resume_path[TRACK_PATH_MAX];
    uint32_t resume_ms;
    int save_session, save_index;
} SaveState;

SaveState *save_state_capture(const Library *lib, const Queue *q,
                             const char *path, uint32_t ms, int index);
void save_state_free(SaveState *s);
int save_state_write(const SaveState *s, const char *session, const char *resume, const char *index);

SaveState *save_state_capture_parts(const Library *lib, const Queue *q, const char *path, uint32_t ms, int session, int index);

enum { SAVE_SESSION_ERROR = 1, SAVE_RESUME_ERROR = 2, SAVE_INDEX_ERROR = 4 };
