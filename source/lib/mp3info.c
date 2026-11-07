#include "mp3info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int BR1[16] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0};
static const int BR2[16] = {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0};

static uint32_t be32(const uint8_t *b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

/* valida header Layer III; devolve tamanho do frame (0 se invalido) */
static int parse_hdr(const uint8_t *h, int *ver, int *br, int *sr, int *ch) {
    if (h[0] != 0xFF || (h[1] & 0xE0) != 0xE0) return 0;
    int v = (h[1] >> 3) & 3, layer = (h[1] >> 1) & 3, bi = (h[2] >> 4) & 15, si = (h[2] >> 2) & 3, pad = (h[2] >> 1) & 1;
    if (v == 1 || layer != 1 || bi == 0 || bi == 15 || si == 3) return 0;
    static const int SR1[3] = {44100, 48000, 32000}, SR2[3] = {22050, 24000, 16000}, SR3[3] = {11025, 12000, 8000};
    *ver = v;
    *br = (v == 3) ? BR1[bi] : BR2[bi];
    *sr = (v == 3) ? SR1[si] : (v == 2) ? SR2[si] : SR3[si];
    *ch = (((h[3] >> 6) & 3) == 3) ? 1 : 2;
    return (v == 3 ? 144 : 72) * (*br) * 1000 / (*sr) + pad;
}

int mp3info_probe(const char *path, long start, Mp3Info *o) {
    memset(o, 0, sizeof *o);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    enum { SCAN = 32768 };
    uint8_t *buf = malloc(SCAN);
    if (!buf) { fclose(f); return -1; }
    fseek(f, start, SEEK_SET);
    size_t n = fread(buf, 1, SCAN, f);
    fclose(f);

    int rc = -2;
    for (size_t i = 0; i + 4 <= n; i++) {
        int ver, br, sr, ch, ver2, br2, sr2, ch2;
        int fl = parse_hdr(buf + i, &ver, &br, &sr, &ch);
        if (!fl) continue;
        if (i + (size_t)fl + 4 <= n) {               /* confirma com o proximo header */
            if (!parse_hdr(buf + i + fl, &ver2, &br2, &sr2, &ch2) || ver2 != ver || sr2 != sr) continue;
        }
        o->sample_rate = sr; o->channels = ch; o->bitrate_kbps = br;
        o->first_frame_off = start + (long)i;
        o->audio_bytes = fsize - o->first_frame_off;

        int side = (ver == 3) ? (ch == 1 ? 17 : 32) : (ch == 1 ? 9 : 17);
        const uint8_t *x = buf + i + 4 + side;
        if (i + 4 + side + 16 <= n && (!memcmp(x, "Xing", 4) || !memcmp(x, "Info", 4))) {
            o->vbr = (x[0] == 'X');
            uint32_t flags = be32(x + 4);
            const uint8_t *p = x + 8;
            if (flags & 1) { o->frames = be32(p); p += 4; }     /* ordem: frames, bytes, toc, qualidade */
            if (flags & 2) p += 4;
            if (flags & 4 && p + 100 <= buf + n) {             /* TOC: byte (0..255) a 0/25..100% do audio */
                memcpy(o->toc, p, 100);
                o->has_toc = 1;
                p += 100;
            }
        } else if (i + 36 + 18 <= n && !memcmp(buf + i + 36, "VBRI", 4)) {
            o->vbr = 1;
            o->frames = be32(buf + i + 36 + 14);
        }
        if (o->frames) {
            uint64_t spf = (ver == 3) ? 1152 : 576;
            o->duration_ms = (uint32_t)((uint64_t)o->frames * spf * 1000 / (uint64_t)sr);
        } else if (br > 0) {
            o->duration_ms = (uint32_t)((uint64_t)o->audio_bytes * 8 / (uint64_t)br);
        }
        rc = 0;
        break;
    }
    free(buf);
    return rc;
}
