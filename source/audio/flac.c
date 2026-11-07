#include "flac.h"
#include "flacinfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLAC_MAX_BLOCK 32768

typedef struct { FILE *f; uint64_t cache; int bits, error; } Bits;
struct FlacDecoder {
    FILE *f; long audio_start, audio_bytes;
    uint32_t dur; uint64_t samples, decoded;
    int hz, channels, bits;
    Bits br;
    int32_t *ch[2], *res;
    int block, pos;
};

static uint32_t bits_read(Bits *b, int n) {
    uint32_t v = 0;
    while (n > 0) {
        if (!b->bits) { int c = fgetc(b->f); if (c == EOF) { b->error = 1; return 0; } b->cache = (unsigned)c; b->bits = 8; }
        int take = n < b->bits ? n : b->bits;
        v = (v << take) | (uint32_t)((b->cache >> (b->bits - take)) & ((1u << take) - 1));
        b->bits -= take; n -= take;
    }
    return v;
}
static int32_t sbits(Bits *b, int n) {
    if (n <= 0) return 0;
    uint32_t v = bits_read(b, n);
    if (v & (1u << (n - 1))) return (int32_t)(v | (~0u << n));
    return (int32_t)v;
}
static void align_bits(Bits *b) { b->bits = 0; }
static uint32_t utf8_num(Bits *b) {
    unsigned char c = (unsigned char)bits_read(b, 8); int n = 1; uint32_t v;
    if ((c & 0xe0) == 0xc0) { n = 2; v = c & 0x1f; }
    else if ((c & 0xf0) == 0xe0) { n = 3; v = c & 0x0f; }
    else if ((c & 0xf8) == 0xf0) { n = 4; v = c & 7; }
    else if ((c & 0xfc) == 0xf8) { n = 5; v = c & 3; }
    else if ((c & 0xfe) == 0xfc) { n = 6; v = c & 1; }
    else return c;
    for (int i = 1; i < n; i++) v = (v << 6) | (bits_read(b, 8) & 0x3f);
    return v;
}
static int block_size(unsigned code, Bits *b) {
    static const int fixed[] = {0, 192, 576, 1152, 2304, 4608, 0, 0, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};
    if (code == 6) return (int)bits_read(b, 8) + 1;
    if (code == 7) return (int)bits_read(b, 16) + 1;
    return code < 15 ? fixed[code] : 0;
}
static int32_t rice(Bits *b, int param) {
    uint32_t q = 0; while (!bits_read(b, 1) && !b->error) q++;
    uint32_t r = param == 0 ? 0 : bits_read(b, param);
    uint32_t u = (q << param) | r;
    return (u & 1) ? -(int32_t)((u + 1) >> 1) : (int32_t)(u >> 1);
}
static int residual(Bits *b, int32_t *dst, int block, int predictor) {
    int method = (int)bits_read(b, 2), pp = method == 0 ? 4 : method == 1 ? 5 : -1;
    if (pp < 0) return -1;
    int order = (int)bits_read(b, 4);
    if (order >= 15) return -1;
    int parts = 1 << order, part_size = block >> order, at = 0;
    for (int p = 0; p < parts; p++) {
        int num = part_size - (p == 0 ? predictor : 0), k = (int)bits_read(b, pp);
        if (k == (1 << pp) - 1) { for (int i = 0; i < num; i++) dst[at++] = sbits(b, 5); }
        else { for (int i = 0; i < num; i++) dst[at++] = rice(b, k); }
    }
    return b->error ? -1 : 0;
}
static int subframe(Bits *b, int32_t *x, int n, int bps) {
    if (bits_read(b, 1)) return -1;
    int type = (int)bits_read(b, 6), wasted = 0;
    if (bits_read(b, 1)) { wasted = 1; while (!bits_read(b, 1) && !b->error) wasted++; bps -= wasted; }
    if (bps <= 0 || bps > 32) return -1;
    if (type == 0) {
        int32_t v = sbits(b, bps) * (1 << wasted);
        for (int i = 0; i < n; i++) x[i] = v;
        return b->error ? -1 : 0;
    }
    if (type == 1) {
        for (int i = 0; i < n; i++) x[i] = sbits(b, bps) * (1 << wasted);
        return b->error ? -1 : 0;
    }
    int order = type >= 8 && type <= 12 ? type - 8 : type >= 32 && type <= 63 ? (type & 31) + 1 : 0;
    if (!order || order >= n) return -1;
    for (int i = 0; i < order; i++) x[i] = sbits(b, bps);
    int32_t coeff[32]; int shift = 0;
    if (type >= 32) {
        int prec = (int)bits_read(b, 4) + 1; if (prec == 16) return -1;
        shift = sbits(b, 5);
        for (int i = 0; i < order; i++) coeff[i] = sbits(b, prec);
    }
    int32_t *r = (int32_t *)malloc(sizeof(int32_t) * (size_t)(n - order));
    if (!r || residual(b, r, n, order) != 0) { free(r); return -1; }
    for (int i = order; i < n; i++) {
        int j = i - order; int64_t pred;
        if (type >= 8 && type <= 12) {
            switch (order) { case 1: pred = x[i-1]; break; case 2: pred = 2LL*x[i-1]-x[i-2]; break;
                case 3: pred = 3LL*x[i-1]-3LL*x[i-2]+x[i-3]; break;
                default: pred = 4LL*x[i-1]-6LL*x[i-2]+4LL*x[i-3]-x[i-4]; break; }
        } else {
            pred = 0; for (int k = 0; k < order; k++) pred += (int64_t)coeff[k] * x[i - k - 1];
            if (shift >= 0) pred >>= shift; else pred <<= -shift;
        }
        int64_t v = pred + r[j]; if (v > INT32_MAX) v = INT32_MAX; if (v < INT32_MIN) v = INT32_MIN; x[i] = (int32_t)v;
    }
    if (wasted) for (int i = 0; i < n; i++) x[i] *= (1 << wasted);
    free(r); return b->error ? -1 : 0;
}
static int next_frame(FlacDecoder *d) {
    Bits *b = &d->br; align_bits(b); int c;
    do { c = fgetc(d->f); if (c == EOF) return 0; } while (c != 0xff);
    int c2 = fgetc(d->f);
    while (c2 != EOF && (c2 & 0xfc) != 0xf8) {
        c2 = c2 == 0xff ? fgetc(d->f) : fgetc(d->f);
    }
    if (c2 == EOF || (c2 & 2)) return 0;
    int h3 = fgetc(d->f);
    if (h3 == EOF) return 0;
    int h4 = fgetc(d->f);
    if (h4 == EOF) return 0;
    int bs = h3 >> 4, sr = h3 & 15;
    int ca = h4 >> 4, sample_size = (h4 >> 1) & 7;
    if (ca > 10 || (ca >= 2 && ca < 8) || (h4 & 1)) return 0;
    if (sample_size == 3 || (sample_size != 0 &&
        sample_size != 1 && sample_size != 2 && sample_size != 4 &&
        sample_size != 5 && sample_size != 6)) return 0;
    b->cache = b->bits = b->error = 0;
    utf8_num(b); /* frame/sample number */
    int n = block_size((unsigned)bs, b);
    if (!n) return 0;
    if (sr == 12) bits_read(b, 8); else if (sr == 13 || sr == 14) bits_read(b, 16);
    bits_read(b, 8); /* header CRC8; channel assignment is not in the frame header */
    if (b->error) return 0;
    if (sample_size && ((sample_size == 1 ? 8 : sample_size == 2 ? 12 :
                        sample_size == 4 ? 16 : sample_size == 5 ? 20 : 24) != d->bits)) return 0;
    int bps = d->bits;
    if (ca < 8 && ca + 1 != d->channels) return 0;
    if (ca == 0) {
        if (subframe(b, d->ch[0], n, bps)) return 0;
    } else if (ca == 1) {
        if (subframe(b, d->ch[0], n, bps) || subframe(b, d->ch[1], n, bps)) return 0;
    } else if (ca == 8) {
        if (subframe(b, d->ch[0], n, bps) || subframe(b, d->ch[1], n, bps + 1)) return 0;
        for (int i = 0; i < n; i++) d->ch[1][i] = d->ch[0][i] - d->ch[1][i];
    } else if (ca == 9) {
        if (subframe(b, d->ch[0], n, bps + 1) || subframe(b, d->ch[1], n, bps)) return 0;
        for (int i = 0; i < n; i++) d->ch[0][i] += d->ch[1][i];
    } else if (ca == 10) {
        if (subframe(b, d->ch[0], n, bps) || subframe(b, d->ch[1], n, bps + 1)) return 0;
        for (int i = 0; i < n; i++) {
            int32_t side = d->ch[1][i];
            int32_t mid = d->ch[0][i] * 2 + (side & 1);
            d->ch[0][i] = (mid + side) >> 1;
            d->ch[1][i] = (mid - side) >> 1;
        }
    } else {
        if (d->channels != 2) return 0;
        if (subframe(b, d->ch[0], n, bps) || subframe(b, d->ch[1], n, bps)) return 0;
    }
    align_bits(b); bits_read(b, 16); /* frame CRC16 */
    d->block = n; d->pos = 0; return 1;
}
static int16_t to16(int32_t v, int bits) {
    int64_t x = bits > 16 ? (int64_t)v >> (bits - 16) : (int64_t)v * (1LL << (16 - bits));
    if (x > 32767) return 32767;
    if (x < -32768) return -32768;
    return (int16_t)x;
}
FlacDecoder *flac_decoder_open(const char *path) {
    FlacInfo i; if (flacinfo_probe(path, &i) != 0) return NULL;
    FlacDecoder *d = calloc(1, sizeof *d); if (!d) return NULL;
    d->f = fopen(path, "rb"); d->audio_start=i.audio_start; d->audio_bytes=i.audio_bytes; d->hz=i.sample_rate; d->channels=i.channels; d->bits=i.bits; d->samples=i.samples;
    d->dur = i.samples ? (uint32_t)(i.samples * 1000 / i.sample_rate) : 0;
    d->ch[0]=malloc(sizeof(int32_t)*FLAC_MAX_BLOCK); d->ch[1]=malloc(sizeof(int32_t)*FLAC_MAX_BLOCK); d->res=NULL;
    if (!d->f || !d->ch[0] || !d->ch[1]) { flac_decoder_close(d); return NULL; }
    fseek(d->f, d->audio_start, SEEK_SET); d->br.f=d->f; return d;
}
void flac_decoder_close(FlacDecoder *d) { if (!d) return; if (d->f) fclose(d->f); free(d->ch[0]); free(d->ch[1]); free(d->res); free(d); }
int flac_decoder_sample_rate(const FlacDecoder *d) { return d->hz; }
uint32_t flac_decoder_duration_ms(const FlacDecoder *d) { return d->dur; }
int flac_decoder_read(FlacDecoder *d, int16_t *out, int max) {
    int done=0; while (done<max) { if (d->pos>=d->block && !next_frame(d)) break; int n=d->block-d->pos; if(n>max-done)n=max-done;
        for(int i=0;i<n;i++){ out[2*(done+i)]=to16(d->ch[0][d->pos+i],d->bits); out[2*(done+i)+1]=d->channels==2?to16(d->ch[1][d->pos+i],d->bits):out[2*(done+i)]; }
        d->pos+=n; done+=n; d->decoded+=n; }
    return done;
}
void flac_decoder_seek_ms(FlacDecoder *d, uint32_t ms) {
    if (d->dur && ms >= d->dur) ms = d->dur - 1;
    /* FLAC frames have variable sizes, so a raw byte estimate can land in
       the middle of a frame (or on a false sync).  Decode headers from the
       beginning and stop at the requested sample. This is slower than an
       indexed seek, but remains small, deterministic and exact enough for
       the player; normal sequential playback is unaffected. */
    uint64_t target = d->samples ? (uint64_t)d->hz * ms / 1000u : 0;
    fseek(d->f, d->audio_start, SEEK_SET);
    d->br.cache = d->br.bits = d->br.error = 0;
    d->block = d->pos = 0;
    d->decoded = 0;
    while (d->decoded < target) {
        if (!next_frame(d)) return;
        if (d->decoded + (uint64_t)d->block > target) {
            d->pos = (int)(target - d->decoded);
            d->decoded = target;
            return;
        }
        d->decoded += (uint64_t)d->block;
        d->pos = d->block;
    }
}
