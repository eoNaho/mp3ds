#pragma once
#include "library.h"
#include "queue.h"

typedef struct {
    Library lib;
    Queue queue;
    char resume_path[256];
    uint32_t resume_ms;
    int save_index;
} SaveState;

SaveState *save_state_capture(const Library *lib, const Queue *q,
                             const char *path, uint32_t ms, int index);
void save_state_free(SaveState *s);
int save_state_write(const SaveState *s, const char *session, const char *resume, const char *index);
