#pragma once
#include <stdint.h>
#include "config.h"

typedef struct {
    char title[TITLE_MAX], artist[META_MAX], album[META_MAX], album_artist[META_MAX];   /* UTF-8 */
    int  track, disc;
    long audio_start;   /* offset do 1o byte depois da tag ID3v2 (0 se nao ha) */
    long art_offset;    /* offset da imagem embutida (APIC/PIC), 0 se nao ha */
    long art_size;
    int  has_v2;
} Id3Info;

/* Le ID3v2.2/2.3/2.4 (e ID3v1 como fallback) sem carregar a capa na memoria. 0 = ok, -1 = nao abriu. */
int id3_read(const char *path, Id3Info *out);
