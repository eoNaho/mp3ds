#include "decoder.h"
#include "id3.h"
#include "mp3info.h"
#include "minimp3.h"
#include "flac.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define IN_CAP (16 * 1024)

struct Decoder {
    int kind;                         /* 0 MP3, 1 FLAC */
    FlacDecoder *flac;
    FILE *f;
    mp3dec_t dec;
    uint8_t *in; int in_len, eof;
    int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int pcm_frames, pcm_pos, channels, hz;
    long audio_start, audio_bytes;
    uint32_t dur;
    uint32_t skip_start, initial_skip;
    uint64_t limit_frames, output_frames;
    int has_toc;
    uint8_t toc[100];     /* Xing TOC: seek mdio em VBR (interpola entre amostras de 1%) */
};

static void refill(Decoder *d) {
    if (d->eof) return;
    size_t n = fread(d->in + d->in_len, 1, IN_CAP - d->in_len, d->f);
    d->in_len += (int)n;
    if (n == 0) d->eof = 1;
}

static void consume(Decoder *d, int n) {
    if (n > d->in_len) n = d->in_len;
    memmove(d->in, d->in + n, d->in_len - n);
    d->in_len -= n;
}

/* decodifica o proximo frame em d->pcm; retorna frames (por canal) ou 0 */
static int decode_next(Decoder *d) {
    for (;;) {
        if (d->in_len < 4096 && !d->eof) refill(d);
        if (d->in_len == 0) return 0;
        mp3dec_frame_info_t fi;
        int samples = mp3dec_decode_frame(&d->dec, d->in, d->in_len, d->pcm, &fi);
        if (fi.frame_bytes == 0) {                   /* falta dado (ou lixo sem sync) */
            if (d->eof) return 0;
            if (d->in_len >= IN_CAP) consume(d, 1); else refill(d);
            continue;
        }
        consume(d, fi.frame_bytes);
        if (samples > 0) {
            d->channels = fi.channels; d->hz = fi.hz;
            d->pcm_frames = samples; d->pcm_pos = 0;
            return samples;
        }
    }
}

Decoder *decoder_open(const char *path) { return decoder_open_cached(path, NULL, 0, 0); }

Decoder *decoder_open_cached(const char *path, const Mp3Info *cached, uint32_t size, int64_t mtime) {
    size_t plen = strlen(path);
#if ENABLE_FLAC
    if (plen >= 5 && !strcasecmp(path + plen - 5, ".flac")) {
        Decoder *d = calloc(1, sizeof *d);
        if (!d) return NULL;
        d->flac = flac_decoder_open(path);
        if (!d->flac) { free(d); return NULL; }
        d->kind = 1;
        d->hz = flac_decoder_sample_rate(d->flac);
        d->dur = flac_decoder_duration_ms(d->flac);
        return d;
    }
#else
    (void)plen;
#endif
    Id3Info id; Mp3Info mi;
    struct stat st;
    if (cached && cached->sample_rate > 0 && stat(path, &st) == 0 &&
        (uint32_t)st.st_size == size && (int64_t)st.st_mtime == mtime &&
        cached->first_frame_off >= 0 && cached->first_frame_off < st.st_size) mi = *cached;
    else {
        if (id3_read(path, &id) != 0) return NULL;
        if (mp3info_probe(path, id.audio_start, &mi) != 0) return NULL;
    }
    Decoder *d = calloc(1, sizeof *d);
    if (!d) return NULL;
    d->in = malloc(IN_CAP);
    d->f = fopen(path, "rb");
    if (!d->in || !d->f) { decoder_close(d); return NULL; }
    d->audio_start = mi.first_frame_off + mi.info_frame_bytes; d->audio_bytes = mi.audio_bytes - mi.info_frame_bytes; d->dur = mi.duration_ms;
    if (mi.has_gapless) {
        /* minimp3 acrescenta 529 amostras de atraso ao atraso do encoder. */
        d->skip_start = d->initial_skip = mi.encoder_delay + 529;
        d->limit_frames = mi.playable_frames;
    }
    d->has_toc = mi.has_toc;
    memcpy(d->toc, mi.toc, sizeof d->toc);
    mp3dec_init(&d->dec);
    fseek(d->f, d->audio_start, SEEK_SET);
    if (!decode_next(d)) { decoder_close(d); return NULL; }   /* ja decodifica 1 frame: define hz/canais */
    return d;
}

void decoder_close(Decoder *d) {
    if (!d) return;
    if (d->kind == 1) { flac_decoder_close(d->flac); free(d); return; }
    if (d->f) fclose(d->f);
    free(d->in); free(d);
}

int decoder_sample_rate(const Decoder *d) { return d->kind == 1 ? flac_decoder_sample_rate(d->flac) : d->hz; }
uint32_t decoder_duration_ms(const Decoder *d) { return d->kind == 1 ? flac_decoder_duration_ms(d->flac) : d->dur; }

int decoder_read(Decoder *d, int16_t *out, int max) {
    if (d->kind == 1) return flac_decoder_read(d->flac, out, max);
    int done = 0;
    while (done < max) {
        if (d->pcm_pos >= d->pcm_frames && !decode_next(d)) break;
        if (d->skip_start) {
            uint32_t skip = (uint32_t)(d->pcm_frames - d->pcm_pos);
            if (skip > d->skip_start) skip = d->skip_start;
            d->pcm_pos += skip; d->skip_start -= skip;
            continue;
        }
        if (d->limit_frames && d->output_frames >= d->limit_frames) break;
        int avail = d->pcm_frames - d->pcm_pos, n = (max - done < avail) ? max - done : avail;
        if (d->limit_frames && (uint64_t)n > d->limit_frames - d->output_frames)
            n = (int)(d->limit_frames - d->output_frames);
        if (d->channels == 1) {
            for (int i = 0; i < n; i++) out[2 * (done + i)] = out[2 * (done + i) + 1] = d->pcm[d->pcm_pos + i];
        } else {
            memcpy(out + 2 * done, d->pcm + 2 * d->pcm_pos, (size_t)n * 4);
        }
        d->pcm_pos += n; done += n; d->output_frames += n;
    }
    return done;
}

/* ms -> offset de byte dentro da regiao de audio: usa a TOC do Xing quando existir */
static long byte_for_ms(const Decoder *d, uint32_t ms) {
    if (d->dur == 0) return 0;
    if (d->has_toc) {
        uint32_t p = (uint32_t)((uint64_t)ms * 100 / d->dur);          /* posicao em % : 0..100 */
        int k = (p >= 100) ? 99 : (int)p;
        uint32_t r = (uint32_t)((uint64_t)ms * 100 % d->dur);          /* avanço dentro do intervalo */
        int b0 = d->toc[k], b1 = k < 99 ? d->toc[k + 1] : 255;
        if (b1 < b0) b1 = b0;
        int v = b0 + (int)((uint64_t)(b1 - b0) * r / d->dur);
        return (long)((uint64_t)d->audio_bytes * (uint64_t)v / 256);
    }
    return (long)((uint64_t)d->audio_bytes * ms / d->dur);
}

void decoder_seek_ms(Decoder *d, uint32_t ms) {
    if (d->kind == 1) { flac_decoder_seek_ms(d->flac, ms); return; }
    if (d->dur == 0) ms = 0; else if (ms >= d->dur) ms = d->dur - 1;
    d->skip_start = ms == 0 ? d->initial_skip : 0;
    d->output_frames = (uint64_t)ms * d->hz / 1000;
    long off = d->audio_start + byte_for_ms(d, ms);
    fseek(d->f, off, SEEK_SET);
    d->in_len = 0; d->eof = 0;
    mp3dec_init(&d->dec);
    d->pcm_frames = d->pcm_pos = 0;
}
