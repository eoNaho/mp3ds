#pragma once
#include "library.h"
#include "queue.h"

/* Guarda caminhos (nao IDs), ordem ouvida, slot atual e modos da fila. */
int session_save(const char *file, const Library *lib, const Queue *q);
int session_load(const char *file, const Library *lib, Queue *q);
