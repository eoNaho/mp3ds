#pragma once
#include <stdint.h>

typedef struct FlacDecoder FlacDecoder;
FlacDecoder *flac_decoder_open(const char *path);
void flac_decoder_close(FlacDecoder *d);
int flac_decoder_sample_rate(const FlacDecoder *d);
uint32_t flac_decoder_duration_ms(const FlacDecoder *d);
int flac_decoder_read(FlacDecoder *d, int16_t *out, int max_frames);
void flac_decoder_seek_ms(FlacDecoder *d, uint32_t ms);
