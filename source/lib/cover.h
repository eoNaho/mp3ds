#pragma once
#include <stdint.h>
#include <stddef.h>
#include "config.h"

typedef struct {
    uint32_t accent;                          /* 0xAABBGGRR em ordem de bytes R,G,B,A (little-endian) */
    uint8_t  px[COVER_SIZE * COVER_SIZE * 4]; /* RGBA8, linha a linha */
} CoverData;

/* Garante o arquivo de cache <cache_dir>/<key>.cvr (capa embutida -> folder.jpg/cover.jpg -> marcador "sem capa").
   Retorna 0 = tem capa, 1 = sem capa, <0 = erro. Pode demorar (decodifica JPEG): rode em thread. */
int cover_ensure(const char *cache_dir, uint32_t key, const char *track_path);
/* Le o cache pronto. 0 = ok, 1 = sem capa, <0 = erro. */
int cover_load(const char *cache_dir, uint32_t key, CoverData *out);

/* Tenta todas as faixas do album antes de criar CVR0. */
int cover_ensure_album(const char *cache_dir, uint32_t key, const char (*paths)[TRACK_PATH_MAX], int count);
int cover_clear_missing(const char *cache_dir);
