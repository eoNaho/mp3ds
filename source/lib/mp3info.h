#pragma once
#include <stdint.h>

typedef struct {
    int      sample_rate, channels, bitrate_kbps, vbr;
    uint32_t frames;          /* 0 se nao ha Xing/VBRI */
    uint32_t duration_ms;
    long     first_frame_off; /* offset do 1o frame valido */
    long     audio_bytes;     /* bytes de audio a partir de first_frame_off */
    uint32_t info_frame_bytes;
    uint32_t encoder_delay, encoder_padding;
    uint64_t playable_frames; /* por canal, apos delay/padding; 0 = desconhecido */
    uint8_t  has_gapless;
    uint8_t  has_toc;         /* 1 = tabela Xing disponivel (seek preciso em VBR) */
    uint8_t  toc[100];        /* bytes = 0..255 ao longo da regiao de audio */
} Mp3Info;

/* Acha o 1o frame MPEG Layer III apos audio_start e estima a duracao (Xing/VBRI ou CBR). 0 = ok */
int mp3info_probe(const char *path, long audio_start, Mp3Info *out);
