#include "id3.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint32_t synchsafe(const uint8_t *b) {
    return ((uint32_t)(b[0] & 0x7f) << 21) | ((uint32_t)(b[1] & 0x7f) << 14) |
           ((uint32_t)(b[2] & 0x7f) << 7) | (b[3] & 0x7f);
}
static uint32_t be32(const uint8_t *b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}
static uint32_t be24(const uint8_t *b) {
    return ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2];
}

static void put_cp(uint32_t cp, char *out, size_t cap, size_t *o) {
    char t[4]; int n;
    if (cp < 0x80)        { t[0] = (char)cp; n = 1; }
    else if (cp < 0x800)  { t[0] = (char)(0xC0 | (cp >> 6)); t[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
    else if (cp < 0x10000){ t[0] = (char)(0xE0 | (cp >> 12)); t[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); t[2] = (char)(0x80 | (cp & 0x3F)); n = 3; }
    else                  { t[0] = (char)(0xF0 | (cp >> 18)); t[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); t[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); t[3] = (char)(0x80 | (cp & 0x3F)); n = 4; }
    if (*o + (size_t)n + 1 > cap) return;
    memcpy(out + *o, t, n); *o += n;
}

/* enc: 0 = ISO-8859-1, 1 = UTF-16 c/ BOM, 2 = UTF-16BE, 3 = UTF-8 */
static void text_to_utf8(const uint8_t *s, size_t n, int enc, char *out, size_t cap) {
    size_t o = 0, i = 0;
    if (enc == 0) {
        for (; i < n && s[i]; i++) put_cp(s[i], out, cap, &o);
    } else if (enc == 3) {
        while (i < n && s[i]) {
            size_t len = s[i] < 0x80 ? 1 : s[i] < 0xE0 ? 2 : s[i] < 0xF0 ? 3 : 4;
            if (i + len > n || o + len >= cap) break;
            memcpy(out + o, s + i, len); o += len; i += len;
        }
    } else {
        int be = (enc == 2);
        if (enc == 1 && n >= 2) {
            if (s[0] == 0xFF && s[1] == 0xFE) { be = 0; i = 2; }
            else if (s[0] == 0xFE && s[1] == 0xFF) { be = 1; i = 2; }
        }
        for (; i + 1 < n; i += 2) {
            uint32_t u = be ? (uint32_t)(s[i] << 8 | s[i + 1]) : (uint32_t)(s[i + 1] << 8 | s[i]);
            if (!u) break;
            if (u >= 0xD800 && u < 0xDC00 && i + 3 < n) {
                uint32_t u2 = be ? (uint32_t)(s[i + 2] << 8 | s[i + 3]) : (uint32_t)(s[i + 3] << 8 | s[i + 2]);
                if (u2 >= 0xDC00 && u2 < 0xE000) { u = 0x10000 + ((u - 0xD800) << 10) + (u2 - 0xDC00); i += 2; }
            }
            put_cp(u, out, cap, &o);
        }
    }
    out[o] = 0;
    while (o > 0 && (out[o - 1] == ' ' || out[o - 1] == '\r' || out[o - 1] == '\n')) out[--o] = 0;
}

static void read_text(FILE *f, long dpos, uint32_t fs, char *dst, size_t cap) {
    uint8_t b[2 * TITLE_MAX + 4];
    size_t n = fs < sizeof b ? fs : sizeof b;
    if (n < 2) return;
    fseek(f, dpos, SEEK_SET);
    if (fread(b, 1, n, f) != n) return;
    text_to_utf8(b + 1, n - 1, b[0], dst, cap);
}

static void read_v1(FILE *f, Id3Info *o) {
    uint8_t b[128];
    if (fseek(f, -128, SEEK_END) != 0) return;
    if (fread(b, 1, 128, f) != 128 || memcmp(b, "TAG", 3)) return;
    if (!o->title[0])  text_to_utf8(b + 3,  30, 0, o->title,  sizeof o->title);
    if (!o->artist[0]) text_to_utf8(b + 33, 30, 0, o->artist, sizeof o->artist);
    if (!o->album[0])  text_to_utf8(b + 63, 30, 0, o->album,  sizeof o->album);
    if (!o->track && b[125] == 0 && b[126]) o->track = b[126];
}

int id3_read(const char *path, Id3Info *o) {
    memset(o, 0, sizeof *o);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    uint8_t h[10];
    if (fread(h, 1, 10, f) == 10 && !memcmp(h, "ID3", 3) && h[3] >= 2 && h[3] <= 4) {
        int ver = h[3], art_type = -1;
        uint8_t flags = h[5];
        uint32_t size = synchsafe(h + 6);
        long pos = 10, end = 10 + (long)size;
        o->has_v2 = 1;
        o->audio_start = end + ((ver == 4 && (flags & 0x10)) ? 10 : 0);

        if (ver >= 3 && (flags & 0x40)) {           /* header estendido */
            uint8_t e[4];
            if (fread(e, 1, 4, f) == 4) pos += (ver == 4) ? (long)synchsafe(e) : (long)be32(e) + 4;
        }
        int idlen = (ver == 2) ? 3 : 4, hlen = (ver == 2) ? 6 : 10;
        while (pos + hlen <= end) {
            uint8_t fh[10];
            uint32_t fs; uint16_t fl = 0;
            fseek(f, pos, SEEK_SET);
            if (fread(fh, 1, hlen, f) != (size_t)hlen) break;
            if (fh[0] == 0) break;                   /* padding */
            if (ver == 2)      fs = be24(fh + 3);
            else if (ver == 3) { fs = be32(fh + 4);      fl = (uint16_t)(fh[8] << 8 | fh[9]); }
            else               { fs = synchsafe(fh + 4); fl = (uint16_t)(fh[8] << 8 | fh[9]); }
            long dpos = pos + hlen;
            if (fs == 0 || dpos + (long)fs > end) break;
            pos = dpos + (long)fs;                   /* proximo frame */

            int bad = (ver == 3) ? (fl & 0x00C0) : (ver == 4) ? (fl & 0x000C) : 0; /* comprimido/cifrado */
            uint32_t extra = 0;
            if (ver == 3 && (fl & 0x0020)) extra = 1;
            if (ver == 4) { if (fl & 0x0040) extra++; if (fl & 0x0001) extra += 4; }
            if (bad || extra >= fs) continue;
            dpos += extra; fs -= extra;

            char id[5] = {0};
            memcpy(id, fh, idlen);
            if (!strcmp(id, "TIT2") || !strcmp(id, "TT2")) read_text(f, dpos, fs, o->title, sizeof o->title);
            else if (!strcmp(id, "TPE1") || !strcmp(id, "TP1")) read_text(f, dpos, fs, o->artist, sizeof o->artist);
            else if (!strcmp(id, "TPE2") || !strcmp(id, "TP2")) read_text(f, dpos, fs, o->album_artist, sizeof o->album_artist);
            else if (!strcmp(id, "TPOS") || !strcmp(id, "TPA")) {
                char t[16] = {0}; read_text(f, dpos, fs, t, sizeof t); o->disc = atoi(t);
            }
            else if (!strcmp(id, "TALB") || !strcmp(id, "TAL")) read_text(f, dpos, fs, o->album, sizeof o->album);
            else if (!strcmp(id, "TRCK") || !strcmp(id, "TRK")) {
                char t[16] = {0};
                read_text(f, dpos, fs, t, sizeof t);
                o->track = atoi(t);                  /* "3/12" -> 3 */
            } else if ((!strcmp(id, "APIC") || !strcmp(id, "PIC")) && art_type != 3) {
                uint8_t b[512];
                size_t n = fs < sizeof b ? fs : sizeof b, p = 1;
                fseek(f, dpos, SEEK_SET);
                if (fread(b, 1, n, f) != n) continue;
                int enc = b[0];
                if (ver == 2) p += 3;                /* formato de 3 letras */
                else { while (p < n && b[p]) p++; p++; }   /* mime */
                if (p >= n) continue;
                int type = b[p++];                /* 3 = capa frontal */
                if (enc == 1 || enc == 2) { while (p + 1 < n && !(b[p] == 0 && b[p + 1] == 0)) p += 2; p += 2; }
                else { while (p < n && b[p]) p++; p++; }   /* descricao */
                if (p <= n && p < fs && (!o->art_offset || type == 3)) { art_type = type; o->art_offset = dpos + (long)p; o->art_size = (long)fs - (long)p; }
            }
        }
    }
    read_v1(f, o);
    fclose(f);
    return 0;
}
