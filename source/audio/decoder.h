#pragma once
#include <stdint.h>

typedef struct Decoder Decoder;

Decoder *decoder_open(const char *path);          /* NULL se nao for MP3 valido; FLAC e experimental */
void     decoder_close(Decoder *d);
int      decoder_sample_rate(const Decoder *d);
uint32_t decoder_duration_ms(const Decoder *d);
/* Preenche out com frames estereo intercalados (L,R). Retorna frames lidos; 0 = fim. */
int      decoder_read(Decoder *d, int16_t *out, int max_frames);
void     decoder_seek_ms(Decoder *d, uint32_t ms);
