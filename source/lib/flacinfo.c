#include "flacinfo.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint32_t be24(const unsigned char *p) { return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2]; }

static void tag(FlacInfo *o, const char *s, size_t n) {
    char b[TITLE_MAX + 32];
    if (n >= sizeof b) {
        n = sizeof b - 1;
        while (n && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
    }
    memcpy(b, s, n); b[n] = 0;
    char *eq = strchr(b, '='); if (!eq) return;
    *eq++ = 0;
    if (!strcasecmp(b, "TITLE")) u_copy(o->title, sizeof o->title, eq);
    else if (!strcasecmp(b, "ARTIST")) u_copy(o->artist, sizeof o->artist, eq);
    else if (!strcasecmp(b, "ALBUM")) u_copy(o->album, sizeof o->album, eq);
    else if (!strcasecmp(b, "ALBUMARTIST")) u_copy(o->album_artist, sizeof o->album_artist, eq);
    else if (!strcasecmp(b, "TRACKNUMBER")) o->track = atoi(eq);
    else if (!strcasecmp(b, "DISCNUMBER")) o->disc = atoi(eq);
}

int flacinfo_probe(const char *path, FlacInfo *o) {
    memset(o, 0, sizeof *o);
    FILE *f = fopen(path, "rb"); if (!f) return -1;
    unsigned char h[4];
    long marker = 0;
    if (fread(h, 1, 4, f) != 4 || memcmp(h, "fLaC", 4)) {
        /* Alguns rippers deixam ID3v2 antes do marcador FLAC. */
        fseek(f, 0, SEEK_SET);
        for (int i = 0; i < 1024 * 1024; i++) {
            int c = fgetc(f); if (c == EOF) break;
            if (c == 'f' && fread(h + 1, 1, 3, f) == 3 && !memcmp(h, "fLaC", 4)) { marker = ftell(f) - 4; break; }
            fseek(f, -3, SEEK_CUR);
        }
        if (marker == 0) { fclose(f); return -2; }
        fseek(f, marker + 4, SEEK_SET);
    }
    int last = 0, got_stream = 0;
    while (!last) {
        unsigned char bh[4];
        if (fread(bh, 1, 4, f) != 4) { fclose(f); return -3; }
        last = bh[0] & 0x80;
        unsigned type = bh[0] & 0x7f, n = be24(bh + 1);
        if (n > 4 * 1024 * 1024) { fclose(f); return -4; }
        unsigned char *b = malloc(n ? n : 1);
        if (!b || fread(b, 1, n, f) != n) { free(b); fclose(f); return -5; }
        if (type == 0 && n >= 34) {
            uint32_t srch = ((uint32_t)b[10] << 12) | ((uint32_t)b[11] << 4) | (b[12] >> 4);
            o->sample_rate = (int)srch;
            o->channels = ((b[12] >> 1) & 7) + 1;
            o->bits = ((b[12] & 1) << 4) | (b[13] >> 4); o->bits++;
            o->samples = ((uint64_t)(b[13] & 0x0f) << 32) | ((uint64_t)b[14] << 24) |
                         ((uint64_t)b[15] << 16) | ((uint64_t)b[16] << 8) | b[17];
            got_stream = 1;
        } else if (type == 4 && n >= 8) {
            uint32_t vendor = le32(b); size_t p = 4;
            if (vendor <= n - p) { p += vendor; if (p + 4 <= n) {
                uint32_t count = le32(b + p); p += 4;
                for (uint32_t i = 0; i < count && p + 4 <= n; i++) {
                    uint32_t len = le32(b + p); p += 4;
                    if (len > n - p) break;
                    tag(o, (const char *)b + p, len); p += len;
                }
            }}
        }
        free(b);
    }
    o->audio_start = ftell(f);
    fseek(f, 0, SEEK_END); long end = ftell(f);
    o->audio_bytes = end > o->audio_start ? end - o->audio_start : 0;
    fclose(f);
    return got_stream && o->sample_rate > 0 && (o->channels == 1 || o->channels == 2) &&
           (o->bits >= 8 && o->bits <= 24) ? 0 : -6;
}
