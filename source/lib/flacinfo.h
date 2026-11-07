#pragma once
#include <stdint.h>

typedef struct {
    char title[96], artist[64], album[64];
    int track;
    int sample_rate, channels, bits;
    uint64_t samples;
    long audio_start, audio_bytes;
} FlacInfo;

/* Lê STREAMINFO/Vorbis Comment sem decodificar áudio. 0 = FLAC válido. */
int flacinfo_probe(const char *path, FlacInfo *out);
