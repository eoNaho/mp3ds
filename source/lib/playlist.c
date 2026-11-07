#include "playlist.h"
#include "util.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int has_ext(const char *n, const char *e) {
    size_t a = strlen(n), b = strlen(e);
    return a > b && !strcasecmp(n + a - b, e);
}

static int cmp_pl(const void *a, const void *b) {
    return strcasecmp(((const PlaylistInfo *)a)->name, ((const PlaylistInfo *)b)->name);
}

/* Remove ., .. and duplicate separators from a playlist path. This matters
   for lists kept in Music/Playlists that use entries such as ../Biblioteca/….
   Keep the sdmc:/ prefix intact because library paths use the same spelling. */
static void normalize_path(char *path) {
    char out[PL_PATH_MAX];
    size_t prefix = 0, o = 0;
    const char *p = path;

    if (!strncmp(p, "sdmc:/", 6)) {
        memcpy(out, p, 6); prefix = o = 6; p += 6;
    } else if (*p == '/') {
        out[0] = '/'; prefix = o = 1; p++;
    }

    while (*p) {
        const char *start;
        size_t n;
        while (*p == '/') p++;
        start = p;
        while (*p && *p != '/') p++;
        n = (size_t)(p - start);
        if (!n || (n == 1 && start[0] == '.')) continue;
        if (n == 2 && start[0] == '.' && start[1] == '.') {
            if (o > prefix) {
                if (out[o - 1] == '/') o--;
                while (o > prefix && out[o - 1] != '/') o--;
            }
            continue;
        }
        if (o > prefix && out[o - 1] != '/') out[o++] = '/';
        if (o + n >= sizeof out) break;
        memcpy(out + o, start, n);
        o += n;
    }
    if (o == 0) out[o++] = '.';
    out[o] = 0;
    u_copy(path, PL_PATH_MAX, out);
}

int playlist_scan(const char *dir, PlaylistInfo *out, int max) {
    DIR *d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while (n < max && (e = readdir(d))) {
        if (!(has_ext(e->d_name, ".m3u") || has_ext(e->d_name, ".m3u8"))) continue;
        if (snprintf(out[n].path, sizeof out[n].path, "%s/%s", dir, e->d_name) >= (int)sizeof out[n].path) continue;
        u_copy(out[n].name, sizeof out[n].name, e->d_name);
        char *dot = strrchr(out[n].name, '.');
        if (dot) *dot = 0;
        n++;
    }
    closedir(d);
    qsort(out, n, sizeof *out, cmp_pl);
    return n;
}

int playlist_load(const char *file, char (*paths)[PL_PATH_MAX], int max) {
    FILE *f = fopen(file, "rb");
    if (!f) return -1;
    char base[PL_PATH_MAX]; u_copy(base, sizeof base, file);
    char *sl = strrchr(base, '/'); if (sl) *sl = 0; else base[0] = 0;

    char line[512]; int n = 0, first = 1;
    while (n < max && fgets(line, sizeof line, f)) {
        char *s = line;
        if (first && (uint8_t)s[0] == 0xEF && (uint8_t)s[1] == 0xBB && (uint8_t)s[2] == 0xBF) s += 3;   /* BOM */
        first = 0;
        size_t l = strlen(s);
        while (l && (s[l - 1] == '\n' || s[l - 1] == '\r' || s[l - 1] == ' ')) s[--l] = 0;
        if (!l || s[0] == '#') continue;
        for (char *p = s; *p; p++) if (*p == '\\') *p = '/';
        int r;
        if (!strncmp(s, "sdmc:/", 6)) r = snprintf(paths[n], PL_PATH_MAX, "%s", s);
        else if (s[0] == '/') r = snprintf(paths[n], PL_PATH_MAX, "sdmc:%s", s);
        else if (base[0]) r = snprintf(paths[n], PL_PATH_MAX, "%s/%s", base, s);
        else r = snprintf(paths[n], PL_PATH_MAX, "%s", s);
        if (r >= PL_PATH_MAX) continue;               /* caminho longo demais: ignora a entrada */
        normalize_path(paths[n]);
        n++;
    }
    fclose(f);
    return n;
}

int playlist_save(const char *file, const char (*paths)[PL_PATH_MAX], int n) {
    FILE *f = fopen(file, "wb");
    if (!f) return -1;
    fputs("#EXTM3U\n", f);
    for (int i = 0; i < n; i++) fprintf(f, "%s\n", paths[i]);
    fclose(f);
    return 0;
}
