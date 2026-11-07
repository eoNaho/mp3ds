#include "util.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

void u_copy(char *dst, size_t cap, const char *src) {
    if (!cap) return;
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
}

uint32_t u_fnv1a(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) { h ^= (uint8_t)*s; h *= 16777619u; }
    return h;
}

int u_mkdir_p(const char *path) {
    char tmp[512];
    u_copy(tmp, sizeof tmp, path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(tmp, 0777); *p = '/'; }
    }
    if (mkdir(tmp, 0777) != 0 && errno != EEXIST) return -1;
    return 0;
}

void u_utf8_trunc(char *dst, size_t cap, const char *src, int max_chars) {
    const unsigned char *s = (const unsigned char *)src;
    size_t o = 0; int n = 0;
    if (!cap) return;
    while (*s) {
        size_t l = 1;
        if (*s >= 0xF0) l = 4; else if (*s >= 0xE0) l = 3; else if (*s >= 0xC0) l = 2;
        for (size_t k = 1; k < l; k++) if (!s[k]) { l = k; break; }
        if (n >= max_chars) { if (o + 4 < cap) { memcpy(dst + o, "...", 3); o += 3; } break; }
        if (o + l + 4 >= cap) break;
        memcpy(dst + o, s, l); o += l; s += l; n++;
    }
    dst[o] = 0;
}

/* tabela para sequencias UTF-8 "C3 xx" (Latin-1 suplemento U+00C0..U+00FF): minusculas sem acento */
static const unsigned char C3_MAP[64] = {
    'a','a','a','a','a','a','a','c','e','e','e','e','i','i','i','i',
    'd','n','o','o','o','o','o',' ', 'o','u','u','u','u','y','t','s',
    'a','a','a','a','a','a','a','c','e','e','e','e','i','i','i','i',
    'd','n','o','o','o','o','o',' ', 'o','u','u','u','u','y','t','y',
};

void u_norm_ascii(char *dst, size_t cap, const char *src) {
    size_t o = 0;
    if (!cap) return;
    for (const unsigned char *s = (const unsigned char *)src; *s; s++) {
        unsigned char c;
        if (*s >= 0x20 && *s < 0x80) {
            c = *s;
            if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            else if (c >= '0' && c <= '9') { }
            else if ((c >= 'a' && c <= 'z') || c == '-') { }
            else c = ' ';
        } else if (*s == 0xC3 && (unsigned char)s[1] >= 0x80 && (unsigned char)s[1] < 0xC0) {
            c = C3_MAP[s[1] - 0x80]; s++;
        } else {                                 /* outros multibyte: destaca palavra */
            c = ' ';
            while ((s[1] & 0xC0) == 0x80) s++;   /* pula os bytes de continuacao 10xxxxxx */
        }
        if (c == ' ' && o > 0 && dst[o - 1] == ' ') continue;   /* colapsa espacos */
        if (o + 2 >= cap) break;
        dst[o++] = c;
    }
    while (o > 0 && dst[o - 1] == ' ') o--;
    dst[o] = 0;
}
