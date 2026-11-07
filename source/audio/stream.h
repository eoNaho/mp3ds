#pragma once
#include "library.h"
#include "decoder.h"

/* Pertence exclusivamente a thread de audio; a UI envia copias de Track. */
typedef struct {
    Decoder *current, *next;
    Track next_track;
    int next_id;
} AudioStream;

int stream_start(AudioStream *s, const Track *track, uint32_t seek_ms);
void stream_close(AudioStream *s);
void stream_prepare(AudioStream *s, const Track *next, int id);
/* Retorna PCM de uma faixa por chamada. started_id recebe o id na transicao,
   ou -1. Taxas diferentes exigem drenar/reconfigurar o DSP pelo chamador. */
int stream_read(AudioStream *s, int16_t *pcm, int frames, int *started_id);
