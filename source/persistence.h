#pragma once
#include "save_state.h"

int persistence_init(void);
int persistence_submit(const Library *lib, const Queue *q, const char *path, uint32_t ms, int index);
int persistence_busy(void);
int persistence_take_error(void);
void persistence_exit(void); /* drena a ultima captura antes de encerrar */

int persistence_submit_parts(const Library *lib, const Queue *q, const char *path, uint32_t ms, int session, int index);
