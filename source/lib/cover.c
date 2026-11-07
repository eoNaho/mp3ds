#include "config.h"
#include "cover.h"
#include "id3.h"
#include "util.h"
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#ifdef __3DS__
#include <3ds.h>
/* New 3DS: clock cheia durante a decodificacao/reducao da capa (por thread) */
#define SPEEDUP_ON()  osSetSpeedupEnable(true)
#define SPEEDUP_OFF() osSetSpeedupEnable(false)
#else
#define SPEEDUP_ON()  ((void)0)
#define SPEEDUP_OFF() ((void)0)
#endif

#define MAX_ART_BYTES   (3 * 1024 * 1024)
#define MAX_ART_PIXELS  (2500 * 1000)   /* limite de RAM durante a decodificacao */

typedef struct { char magic[4]; uint32_t w, h, accent; } CvrHeader;

static void cache_path(char *o, size_t n, const char *dir, uint32_t key) { snprintf(o, n, "%s/%08x.cvr", dir, (unsigned)key); }

/* recorta quadrado central e reduz por media de area para COVER_SIZE; calcula cor de destaque */
static void shrink(const uint8_t *src, int w, int h, CoverData *o) {
    int side = w < h ? w : h, x0 = (w - side) / 2, y0 = (h - side) / 2;
    double wr = 0, wg = 0, wb = 0, wt = 0, ar = 0, ag = 0, ab = 0;
    for (int dy = 0; dy < COVER_SIZE; dy++) {
        int sy0 = y0 + dy * side / COVER_SIZE, sy1 = y0 + (dy + 1) * side / COVER_SIZE; if (sy1 <= sy0) sy1 = sy0 + 1;
        for (int dx = 0; dx < COVER_SIZE; dx++) {
            int sx0 = x0 + dx * side / COVER_SIZE, sx1 = x0 + (dx + 1) * side / COVER_SIZE; if (sx1 <= sx0) sx1 = sx0 + 1;
            unsigned r = 0, g = 0, b = 0, cnt = 0;
            for (int y = sy0; y < sy1; y++) for (int x = sx0; x < sx1; x++) {
                const uint8_t *p = src + ((size_t)y * w + x) * 4; r += p[0]; g += p[1]; b += p[2]; cnt++;
            }
            uint8_t *d = o->px + ((size_t)dy * COVER_SIZE + dx) * 4;
            d[0] = (uint8_t)(r / cnt); d[1] = (uint8_t)(g / cnt); d[2] = (uint8_t)(b / cnt); d[3] = 255;
            int mx = d[0] > d[1] ? (d[0] > d[2] ? d[0] : d[2]) : (d[1] > d[2] ? d[1] : d[2]);
            int mn = d[0] < d[1] ? (d[0] < d[2] ? d[0] : d[2]) : (d[1] < d[2] ? d[1] : d[2]);
            double sat = mx ? (double)(mx - mn) / mx : 0.0, wgt = sat * sat * (mx > 40 ? 1.0 : 0.1);
            wr += d[0] * wgt; wg += d[1] * wgt; wb += d[2] * wgt; wt += wgt;
            ar += d[0]; ag += d[1]; ab += d[2];
        }
    }
    double n = COVER_SIZE * COVER_SIZE, r, g, b;
    if (wt > n * 0.02) { r = wr / wt; g = wg / wt; b = wb / wt; } else { r = ar / n; g = ag / n; b = ab / n; }
    double mx = r > g ? (r > b ? r : b) : (g > b ? g : b);   /* acento claro o bastante para fundo escuro */
    if (mx < 150 && mx > 1) { double k = 150.0 / mx; r *= k; g *= k; b *= k; }
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    uint8_t *a = (uint8_t *)&o->accent;
    a[0] = (uint8_t)r; a[1] = (uint8_t)g; a[2] = (uint8_t)b; a[3] = 255;
}

static uint8_t *decode_mem(const uint8_t *buf, int len, int *w, int *h) {
    int c;
    if (!stbi_info_from_memory(buf, len, w, h, &c) || (long)*w * *h > MAX_ART_PIXELS) return NULL;
    return stbi_load_from_memory(buf, len, w, h, &c, 4);
}

static uint8_t *load_embedded(const char *track, int *w, int *h) {
    Id3Info id;
    if (id3_read(track, &id) != 0 || id.art_offset <= 0 || id.art_size <= 16 || id.art_size > MAX_ART_BYTES) return NULL;
    FILE *f = fopen(track, "rb");
    if (!f) return NULL;
    uint8_t *buf = malloc((size_t)id.art_size);
    uint8_t *px = NULL;
    if (buf && fseek(f, id.art_offset, SEEK_SET) == 0 && fread(buf, 1, (size_t)id.art_size, f) == (size_t)id.art_size)
        px = decode_mem(buf, (int)id.art_size, w, h);
    free(buf); fclose(f);
    return px;
}

static uint8_t *load_folder_art(const char *track, int *w, int *h) {
    static const char *names[] = {"cover.jpg", "folder.jpg", "front.jpg", "Cover.jpg", "Folder.jpg", "cover.png", "folder.png", "front.png", "Cover.png", "Folder.png"};
    char dir[TRACK_PATH_MAX]; u_copy(dir, sizeof dir, track);
    char *s = strrchr(dir, '/'); if (s) *s = 0;
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        char p[TRACK_PATH_MAX + 16]; snprintf(p, sizeof p, "%s/%s", dir, names[i]);
        FILE *f = fopen(p, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t *buf = (n > 16 && n <= MAX_ART_BYTES) ? malloc((size_t)n) : NULL, *px = NULL;
        if (buf && fread(buf, 1, (size_t)n, f) == (size_t)n) px = decode_mem(buf, (int)n, w, h);
        free(buf); fclose(f);
        if (px) return px;
    }
    return NULL;
}

int cover_ensure(const char *cache_dir, uint32_t key, const char *track) {
    char paths[1][TRACK_PATH_MAX];
    u_copy(paths[0], sizeof paths[0], track);
    return cover_ensure_album(cache_dir, key, (const char (*)[TRACK_PATH_MAX])paths, 1);
}

int cover_ensure_album(const char *cache_dir, uint32_t key, const char (*paths)[TRACK_PATH_MAX], int count) {
    char cp[300]; cache_path(cp, sizeof cp, cache_dir, key);
    FILE *f = fopen(cp, "rb");
    if (f) { CvrHeader h; int ok = fread(&h, sizeof h, 1, f) == 1; fclose(f); if (ok && !memcmp(h.magic, "CVR0", 4)) return 1;
        if (ok && (!memcmp(h.magic, "CVR1", 4) || !memcmp(h.magic, "CVR2", 4)) && h.w == COVER_SIZE && h.h == COVER_SIZE) return 0; }

    int w = 0, h = 0;
    SPEEDUP_ON();
    uint8_t *px = NULL;
    for (int i = 0; i < count && !px; i++) {
        px = load_embedded(paths[i], &w, &h);
        if (!px) px = load_folder_art(paths[i], &w, &h);
    }

    CvrHeader hd = { {'C', 'V', 'R', '0'}, 0, 0, 0 };
    CoverData *cd = NULL;
    if (px) {
        cd = malloc(sizeof *cd);
        if (cd) { shrink(px, w, h, cd); memcpy(hd.magic, "CVR2", 4); hd.w = hd.h = COVER_SIZE; hd.accent = cd->accent; }
        stbi_image_free(px);
    }
    SPEEDUP_OFF();
    u_mkdir_p(cache_dir);
    f = fopen(cp, "wb");
    if (!f) { free(cd); return -1; }
    int ok = fwrite(&hd, sizeof hd, 1, f) == 1;
    if (cd) {
        /* RGB565 little endian: 32 KiB por capa em vez de 64 KiB. */
        uint8_t row[COVER_SIZE * 2];
        for (int y = 0; y < COVER_SIZE && ok; y++) {
            for (int x = 0; x < COVER_SIZE; x++) {
                const uint8_t *p = cd->px + (y * COVER_SIZE + x) * 4;
                uint16_t rgb = ((p[0] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[2] >> 3);
                row[2*x] = rgb; row[2*x+1] = rgb >> 8;
            }
            ok = fwrite(row, 1, sizeof row, f) == sizeof row;
        }
    }
    if (fclose(f) != 0) ok = 0;
    free(cd);
    if (!ok) { remove(cp); return -1; }
    return hd.magic[3] == '2' ? 0 : 1;
}

int cover_load(const char *cache_dir, uint32_t key, CoverData *o) {
    char cp[300]; cache_path(cp, sizeof cp, cache_dir, key);
    FILE *f = fopen(cp, "rb");
    if (!f) return -1;
    CvrHeader h; int rc = -2;
    if (fread(&h, sizeof h, 1, f) == 1) {
        if (!memcmp(h.magic, "CVR0", 4)) rc = 1;
        else if (!memcmp(h.magic, "CVR2", 4) && h.w == COVER_SIZE && h.h == COVER_SIZE) {
            uint8_t row[COVER_SIZE * 2];
            rc = 0; o->accent = h.accent;
            for (int y = 0; y < COVER_SIZE; y++) {
                if (fread(row, 1, sizeof row, f) != sizeof row) { rc = -2; break; }
                for (int x = 0; x < COVER_SIZE; x++) {
                    uint16_t rgb = row[2*x] | (row[2*x+1] << 8);
                    uint8_t *p = o->px + (y * COVER_SIZE + x) * 4;
                    unsigned r = rgb >> 11, g = (rgb >> 5) & 63, b = rgb & 31;
                    p[0] = (r << 3) | (r >> 2); p[1] = (g << 2) | (g >> 4); p[2] = (b << 3) | (b >> 2); p[3] = 255;
                }
            }
        }
        else if (!memcmp(h.magic, "CVR1", 4) && h.w == COVER_SIZE && h.h == COVER_SIZE && fread(o->px, 1, sizeof o->px, f) == sizeof o->px) { o->accent = h.accent; rc = 0; }
    }
    fclose(f);
    return rc;
}

int cover_clear_missing(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return 0;
    int cleared = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n < 4 || strcmp(e->d_name + n - 4, ".cvr")) continue;
        char path[TRACK_PATH_MAX + 32];
        if (snprintf(path, sizeof path, "%s/%s", dir, e->d_name) >= (int)sizeof path) continue;
        FILE *f = fopen(path, "rb"); if (!f) continue;
        char magic[4]; int missing = fread(magic, 1, 4, f) == 4 && !memcmp(magic, "CVR0", 4);
        fclose(f);
        if (missing && remove(path) == 0) cleared++;
    }
    closedir(d);
    return cleared;
}
