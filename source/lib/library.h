#pragma once
#include <stdint.h>

typedef struct {
    char     path[256];
    char     title[96], artist[64], album[64];
    uint32_t track, duration_ms, art_key, size;
    int64_t  mtime;
} Track;

/* agrupamento de faixas consecutivas (biblioteca deve estar ordenada) */
typedef struct {
    char     artist[64], album[64];
    uint32_t art_key;
    int      first, count;   /* faixas de [first, first+count) na biblioteca */
} AlbumGroup;

typedef struct {
    Track *tracks;
    int count, cap;
    int last_parsed;   /* quantas faixas o ultimo scan precisou ler de novo (as outras vieram do indice) */
} Library;

/* Retorne != 0 para cancelar o scan. */
typedef int (*ScanProgress)(int found, const char *current, void *ud);

void library_free(Library *l);
int  library_load_index(Library *l, const char *idx_path);       /* 0 = ok */
int  library_save_index(const Library *l, const char *idx_path); /* 0 = ok */
/* Varre root recursivamente (so .mp3). Incremental: reaproveita registros com mesmo tamanho+mtime. */
int  library_scan(Library *l, const char *root, ScanProgress cb, void *ud);
void library_sort(Library *l);                                    /* artista, album, faixa, titulo */
int  library_find_path(const Library *l, const char *path);       /* indice ou -1 */
/* Busca texto: substrings em titulo/artista/album, ignorando caixa e acentos.
   Preenche out (max) com indices de faixa em ordem da biblioteca; retorna a quantidade. */
int  library_search(const Library *l, const char *query, int *out, int max);
/* Agrupa as faixas ordenadas em albuns (artista+album). out deve ter espaco para l->count. */
int  library_build_albums(const Library *l, AlbumGroup *out, int max);
