#pragma once
#include "config.h"
#define PL_PATH_MAX TRACK_PATH_MAX

typedef struct { char name[64]; char path[PL_PATH_MAX]; } PlaylistInfo;

int playlist_scan(const char *dir, PlaylistInfo *out, int max);               /* lista arquivos .m3u e .m3u8, ordenado; retorna n */
int playlist_load(const char *file, char (*paths)[PL_PATH_MAX], int max);     /* resolve caminhos relativos; retorna n ou -1 */
int playlist_save(const char *file, const char (*paths)[PL_PATH_MAX], int n); /* 0 = ok */
