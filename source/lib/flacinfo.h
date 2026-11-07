#pragma once
#include <stdint.h>
#include "config.h"

typedef struct {
    char title[TITLE_MAX], artist[META_MAX], album[META_MAX], album_artist[META_MAX];
    int track, disc;
    int sample_rate, channels, bits;
    uint64_t samples;
    long audio_start, audio_bytes;
} FlacInfo;

/* Lê STREAMINFO/Vorbis Comment sem decodificar áudio. 0 = FLAC válido. */
int flacinfo_probe(const char *path, FlacInfo *out);
