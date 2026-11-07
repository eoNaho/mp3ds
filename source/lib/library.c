#include "library.h"
#include "id3.h"
#include "mp3info.h"
#include "flacinfo.h"
#include "util.h"
#include "config.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#define IDX_MAGIC "M3DX"
#define IDX_VERSION 3
#define MAX_DEPTH 16

typedef struct { char magic[4]; uint32_t version, rec_size, count; } IdxHeader;
typedef struct {
    char path[256];
    char title[96], artist[64], album[64];
    uint32_t track, duration_ms, art_key, size;
    int64_t mtime;
} LegacyTrack;

typedef struct {
    char path[256];
    char title[96], artist[64], album[64];
    uint32_t track, duration_ms, art_key, size;
    int64_t mtime, added;
    uint32_t plays;
    int64_t last_played;
} LegacyTrackV2;

struct LibraryLookup { size_t cap; int *slots; };
struct LibrarySearch { size_t *offsets; char *data; };

static void free_caches(Library *l) {
    if (l->lookup) { free(l->lookup->slots); free(l->lookup); l->lookup = NULL; }
    if (l->search) { free(l->search->offsets); free(l->search->data); free(l->search); l->search = NULL; }
}
void library_free(Library *l) { free_caches(l); free(l->tracks); memset(l, 0, sizeof *l); }

const char *track_album_artist(const Track *t) { return t->album_artist[0] ? t->album_artist : t->artist; }

static const char *path_key(const Track *t, int kind) {
    const char *p = t->path;
    if (kind == 2) { const char *b = strrchr(p, '/'); if (b) p = b + 1; }
    return p;
}
static uint32_t path_hash(const char *p, int fold) {
    uint32_t h = 2166136261u;
    for (; *p; p++) { unsigned char c = *p; if (fold && c >= 'A' && c <= 'Z') c += 32; h = (h ^ c) * 16777619u; }
    return h;
}
static int path_equal(const char *a, const char *b, int fold) { return fold ? !strcasecmp(a, b) : !strcmp(a, b); }

void library_prepare(Library *l) {
    free_caches(l);
    if (l->count <= 0) return;
    LibraryLookup *h = calloc(1, sizeof *h);
    if (h) {
        h->cap = 16;
        while (h->cap < (size_t)l->count * 2) h->cap *= 2;
        h->slots = calloc(h->cap * 3, sizeof(int));
        if (!h->slots) { free(h); h = NULL; }
    }
    if (h) for (int kind = 0; kind < 3; kind++) for (int i = 0; i < l->count; i++) {
        const char *key = path_key(&l->tracks[i], kind);
        size_t slot = path_hash(key, kind != 0) & (h->cap - 1);
        int *table = h->slots + kind * h->cap;
        while (table[slot]) {
            int old = abs(table[slot]) - 1;
            if (path_equal(path_key(&l->tracks[old], kind), key, kind != 0)) break;
            slot = (slot + 1) & (h->cap - 1);
        }
        if (!table[slot]) table[slot] = i + 1;
        else if (kind == 2) table[slot] = -abs(table[slot]); /* basename ambiguo */
    }
    l->lookup = h;
    LibrarySearch *c = calloc(1, sizeof *c);
    if (!c) return;
    size_t bytes = 0;
    for (int i = 0; i < l->count; i++) {
        const Track *t = &l->tracks[i];
        bytes += strlen(t->title) + strlen(t->artist) + strlen(t->album) + strlen(t->path) + 4;
    }
    c->offsets = malloc(sizeof(size_t) * (size_t)l->count * 4);
    c->data = malloc(bytes);
    if (!c->offsets || !c->data) { free(c->offsets); free(c->data); free(c); return; }
    size_t pos = 0;
    for (int i = 0; i < l->count; i++) {
        const Track *t = &l->tracks[i];
        char folder[TRACK_PATH_MAX]; u_copy(folder, sizeof folder, t->path);
        char *slash = strrchr(folder, '/'); if (slash) *slash = 0; else folder[0] = 0;
        const char *fields[] = {t->title, t->artist, t->album, folder};
        for (int k = 0; k < 4; k++) {
            c->offsets[i * 4 + k] = pos;
            u_norm_ascii(c->data + pos, strlen(fields[k]) + 1, fields[k]);
            pos += strlen(c->data + pos) + 1;
        }
    }
    l->search = c;
}

static int push(Library *l, const Track *t) {
    if (l->count == l->cap) {
        int nc = l->cap ? l->cap * 2 : 256;
        Track *n = realloc(l->tracks, sizeof(Track) * nc);
        if (!n) return -1;
        l->tracks = n; l->cap = nc;
    }
    l->tracks[l->count++] = *t;
    return 0;
}

int library_load_index(Library *l, const char *p) {
    FILE *f = fopen(p, "rb");
    if (!f) return -1;
    IdxHeader h;
    if (fread(&h, sizeof h, 1, f) != 1 || memcmp(h.magic, IDX_MAGIC, 4) ||
        !((h.version == IDX_VERSION && h.rec_size == sizeof(Track)) ||
          (h.version == 2 && h.rec_size == sizeof(LegacyTrackV2)) ||
          (h.version == 1 && h.rec_size == sizeof(LegacyTrack))) ||
        h.count > 100000) { fclose(f); return -2; }
    Library fresh = {0};
    fresh.tracks = calloc(h.count ? h.count : 1, sizeof(Track));
    if (!fresh.tracks) { fclose(f); return -3; }
    fresh.cap = h.count ? (int)h.count : 1;
    fresh.needs_upgrade = h.version != IDX_VERSION;
    for (uint32_t i = 0; i < h.count; i++) {
        Track *t = &fresh.tracks[i];
        if (h.version == IDX_VERSION) {
            if (fread(t, sizeof *t, 1, f) != 1) goto bad;
            t->path[sizeof t->path - 1] = t->title[sizeof t->title - 1] = 0;
            t->artist[sizeof t->artist - 1] = t->album[sizeof t->album - 1] = t->album_artist[sizeof t->album_artist - 1] = 0;
        } else {
            LegacyTrackV2 old = {0};
            if (h.version == 2) { if (fread(&old, sizeof old, 1, f) != 1) goto bad; }
            else {
                LegacyTrack v1;
                if (fread(&v1, sizeof v1, 1, f) != 1) goto bad;
                memcpy(&old, &v1, sizeof v1); old.added = old.mtime;
            }
            old.path[sizeof old.path - 1] = old.title[sizeof old.title - 1] = 0;
            old.artist[sizeof old.artist - 1] = old.album[sizeof old.album - 1] = 0;
            u_copy(t->path, sizeof t->path, old.path); u_copy(t->title, sizeof t->title, old.title);
            u_copy(t->artist, sizeof t->artist, old.artist); u_copy(t->album, sizeof t->album, old.album);
            t->track = old.track; t->duration_ms = old.duration_ms; t->art_key = old.art_key;
            t->size = old.size; t->mtime = old.mtime; t->added = old.added;
            t->plays = old.plays; t->last_played = old.last_played;
        }
        fresh.count++;
    }
    fclose(f); library_free(l); *l = fresh; library_prepare(l);
    return 0;
bad:
    fclose(f); library_free(&fresh); return -4;
}

int library_save_index(const Library *l, const char *p) {
    char tmp[512];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", p) >= (int)sizeof tmp) return -1;
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    IdxHeader h; memcpy(h.magic, IDX_MAGIC, 4);
    h.version = IDX_VERSION; h.rec_size = sizeof(Track); h.count = (uint32_t)l->count;
    int ok = fwrite(&h, sizeof h, 1, f) == 1;
    if (ok && l->count) ok = fwrite(l->tracks, sizeof(Track), l->count, f) == (size_t)l->count;
    if (fclose(f) != 0) ok = 0;
    if (!ok || rename(tmp, p) != 0) { remove(tmp); return -1; }
    return 0;
}

static int cmp_path(const void *a, const void *b) { return strcmp(((const Track *)a)->path, ((const Track *)b)->path); }

static int cmp_sort(const void *pa, const void *pb) {
    const Track *a = pa, *b = pb; int r;
    if ((r = strcasecmp(track_album_artist(a), track_album_artist(b)))) return r;
    if ((r = strcasecmp(a->album, b->album))) return r;
    if (a->disc != b->disc) return a->disc < b->disc ? -1 : 1;
    if (a->track != b->track) return a->track < b->track ? -1 : 1;
    return strcasecmp(a->title, b->title);
}

static SortMode s_sort_mode;
static const Library *s_sort_lib;

static int cmp_id(const void *pa, const void *pb) {
    const Track *a = &s_sort_lib->tracks[*(const int *)pa];
    const Track *b = &s_sort_lib->tracks[*(const int *)pb];
    int r = 0;
    switch (s_sort_mode) {
        case SORT_ALBUM:
            if ((r = strcasecmp(a->album, b->album))) break;
            if ((r = strcasecmp(track_album_artist(a), track_album_artist(b)))) break;
            if (a->disc != b->disc) return a->disc < b->disc ? -1 : 1;
            if (a->track != b->track) return a->track < b->track ? -1 : 1;
            r = strcasecmp(a->title, b->title); break;
        case SORT_TITULO:
            if ((r = strcasecmp(a->title, b->title))) break;
            r = strcasecmp(track_album_artist(a), track_album_artist(b)); break;
        case SORT_ADICAO:
            if (a->added != b->added) return a->added > b->added ? -1 : 1;
            r = strcasecmp(a->title, b->title); break;
        case SORT_RECENTES:
            if (a->last_played != b->last_played) return a->last_played > b->last_played ? -1 : 1;
            r = strcasecmp(a->title, b->title); break;
        case SORT_ARTISTA:
        default:
            r = cmp_sort(a, b); break;
    }
    if (r) return r;
    return strcmp(a->path, b->path);
}

void library_sort(Library *l) { if (l->count > 1) qsort(l->tracks, l->count, sizeof(Track), cmp_sort); library_prepare(l); }

void library_sort_ids(const Library *l, int *ids, int count, SortMode mode) {
    s_sort_lib = l; s_sort_mode = mode;
    if (count > 1) qsort(ids, (size_t)count, sizeof *ids, cmp_id);
    s_sort_lib = NULL;
}

void library_record_play(Library *l, int id, int64_t when) {
    if (id < 0 || id >= l->count) return;
    if (l->tracks[id].plays != UINT32_MAX) l->tracks[id].plays++;
    l->tracks[id].last_played = when > 0 ? when : (int64_t)time(NULL);
}

int library_find_path(const Library *l, const char *path) {
    if (l->lookup) {
        const LibraryLookup *h = l->lookup;
        for (int kind = 0; kind < 3; kind++) {
            const char *key = path;
            if (kind == 2) { key = strrchr(path, '/'); if (!key || !key[1]) break; key++; }
            const int *table = h->slots + kind * h->cap;
            size_t slot = path_hash(key, kind != 0) & (h->cap - 1);
            while (table[slot]) {
                int id = abs(table[slot]) - 1;
                if (path_equal(path_key(&l->tracks[id], kind), key, kind != 0)) {
                    if (table[slot] > 0) return id;
                    break;
                }
                slot = (slot + 1) & (h->cap - 1);
            }
        }
        return -1;
    }
    for (int i = 0; i < l->count; i++) if (!strcmp(l->tracks[i].path, path)) return i;
    /* Playlists podem ter vindo de outro sistema de arquivos e diferir apenas
       em maiusculas/minusculas (o SD do 3DS nao diferencia esses nomes). */
    for (int i = 0; i < l->count; i++) if (!strcasecmp(l->tracks[i].path, path)) return i;

    /* Se a biblioteca foi copiada sem a pasta intermediaria (por exemplo,
       o conteudo de Music/Biblioteca diretamente para Music), o nome do arquivo continua
       identificando a faixa. So usamos esse fallback quando ha uma unica
       correspondencia, para nao escolher uma faixa errada em duplicatas. */
    const char *base = strrchr(path, '/');
    if (!base || !base[1]) return -1;
    int found = -1;
    for (int i = 0; i < l->count; i++) {
        const char *candidate = strrchr(l->tracks[i].path, '/');
        if (candidate && !strcasecmp(candidate + 1, base + 1)) {
            if (found >= 0) return -1;
            found = i;
        }
    }
    return found;
}

int library_search_filtered(const Library *l, const char *query, SearchField field,
                            SearchPlays plays, int *out, int max) {
    char q[128];
    u_norm_ascii(q, sizeof q, query ? query : "");
    char *terms[64]; int nt = 0;
    char *p = q;
    while (*p && nt < 64) {
        while (*p == ' ') p++;
        if (!*p) break;
        terms[nt++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    int n = 0;
    for (int i = 0; i < l->count && n < max; i++) {
        const Track *t = &l->tracks[i];
        if ((plays == SEARCH_UNPLAYED && t->plays) || (plays == SEARCH_PLAYED && !t->plays)) continue;
        char title[TITLE_MAX] = "", artist[META_MAX] = "", album[META_MAX] = "", folder[TRACK_PATH_MAX] = "";
        const char *fields[4] = {title, artist, album, folder};
        if (l->search) {
            for (int k = 0; k < 4; k++) fields[k] = l->search->data + l->search->offsets[i * 4 + k];
        } else {
            u_norm_ascii(title, sizeof title, t->title); u_norm_ascii(artist, sizeof artist, t->artist);
            u_norm_ascii(album, sizeof album, t->album);
            char path[TRACK_PATH_MAX]; u_copy(path, sizeof path, t->path);
            char *slash = strrchr(path, '/'); if (slash) *slash = 0; else path[0] = 0;
            u_norm_ascii(folder, sizeof folder, path);
        }
        int match = 1;
        for (int k = 0; k < nt; k++) {
            int hit = 0;
            for (int f = 0; f < 4; f++) {
                if ((field == SEARCH_ALL && f < 3) || (int)field == f + 1)
                    if (strstr(fields[f], terms[k])) hit = 1;
            }
            if (!hit) { match = 0; break; }
        }
        if (match) out[n++] = i;
    }
    return n;
}

int library_search(const Library *l, const char *query, int *out, int max) {
    char q[128]; u_norm_ascii(q, sizeof q, query ? query : "");
    if (!q[0]) return 0;
    return library_search_filtered(l, query, SEARCH_ALL, SEARCH_ANY_PLAY, out, max);
}

int library_build_albums(const Library *l, AlbumGroup *out, int max) {
    int n = 0;
    memset(out, 0, sizeof(AlbumGroup) * (size_t)max);
    for (int i = 0; i < l->count; i++) {
        const Track *t = &l->tracks[i];
        int same = n > 0 && !strcasecmp(out[n - 1].artist, track_album_artist(t)) && !strcasecmp(out[n - 1].album, t->album);
        if (same) { out[n - 1].count++; continue; }
        if (n >= max) break;
        u_copy(out[n].artist, sizeof out[n].artist, track_album_artist(t));
        u_copy(out[n].album, sizeof out[n].album, t->album);
        out[n].art_key = t->art_key;
        out[n].first = i; out[n].count = 1;
        n++;
    }
    return n;
}

static int is_mp3(const char *n) { size_t a = strlen(n); return a > 4 && !strcasecmp(n + a - 4, ".mp3"); }
static int is_flac(const char *n) { size_t a = strlen(n); return ENABLE_FLAC && a > 5 && !strcasecmp(n + a - 5, ".flac"); }
static int is_audio(const char *n) { return is_mp3(n) || is_flac(n); }

static void dir_of(const char *path, char *out, size_t cap) {
    u_copy(out, cap, path);
    char *s = strrchr(out, '/'); if (s) *s = 0;
}

static int build_track(const char *path, const struct stat *st, Track *t) {
    if (is_flac(path)) {
        FlacInfo fi;
        if (flacinfo_probe(path, &fi) != 0) return -1;
        memset(t, 0, sizeof *t); u_copy(t->path, sizeof t->path, path);
        if (fi.title[0]) u_copy(t->title, sizeof t->title, fi.title);
        else {
            const char *b = strrchr(path, '/'); b = b ? b + 1 : path;
            u_copy(t->title, sizeof t->title, b); char *dot = strrchr(t->title, '.'); if (dot) *dot = 0;
        }
        u_copy(t->artist, sizeof t->artist, fi.artist[0] ? fi.artist : "Desconhecido");
        u_copy(t->album, sizeof t->album, fi.album);
        t->track = (uint32_t)fi.track;
        t->disc = fi.disc > 0 ? (uint32_t)fi.disc : 1;
        u_copy(t->album_artist, sizeof t->album_artist, fi.album_artist);
        t->duration_ms = fi.sample_rate ? (uint32_t)(fi.samples * 1000 / fi.sample_rate) : 0;
        t->size = (uint32_t)st->st_size; t->mtime = (int64_t)st->st_mtime; t->added = (int64_t)time(NULL);
        if (fi.album[0]) { char k[2 * META_MAX + 2]; snprintf(k, sizeof k, "%s\x1f%s", t->album, track_album_artist(t)); t->art_key = u_fnv1a(k); }
        else { char d[TRACK_PATH_MAX]; dir_of(path, d, sizeof d); t->art_key = u_fnv1a(d); }
        return 0;
    }
    Id3Info id; Mp3Info mi;
    if (id3_read(path, &id) != 0) return -1;
    if (mp3info_probe(path, id.audio_start, &mi) != 0) return -1;   /* nao e MP3 tocavel */
    memset(t, 0, sizeof *t);
    u_copy(t->path, sizeof t->path, path);
    if (id.title[0]) u_copy(t->title, sizeof t->title, id.title);
    else {
        const char *b = strrchr(path, '/'); b = b ? b + 1 : path;
        u_copy(t->title, sizeof t->title, b);
        char *dot = strrchr(t->title, '.'); if (dot) *dot = 0;
    }
    u_copy(t->artist, sizeof t->artist, id.artist[0] ? id.artist : "Desconhecido");
    u_copy(t->album, sizeof t->album, id.album[0] ? id.album : "");
    u_copy(t->album_artist, sizeof t->album_artist, id.album_artist);
    t->disc = id.disc > 0 ? (uint32_t)id.disc : 1;
    t->mp3 = mi;
    t->track = (uint32_t)id.track; t->duration_ms = mi.duration_ms;
    t->size = (uint32_t)st->st_size; t->mtime = (int64_t)st->st_mtime;
    t->added = (int64_t)time(NULL);
    if (id.album[0]) { char k[2 * META_MAX + 2]; snprintf(k, sizeof k, "%s\x1f%s", t->album, track_album_artist(t)); t->art_key = u_fnv1a(k); }
    else { char d[TRACK_PATH_MAX]; dir_of(path, d, sizeof d); t->art_key = u_fnv1a(d); }
    return 0;
}

typedef struct {
    Library *out; const Track *old; int nold;
    ScanProgress cb; void *ud; int abort, parsed;
    Track key, tmp;   /* buffers compartilhados: evita ~1 KB de pilha por nivel de recursao (pilha do 3DS e pequena) */
} Scan;

static void walk(Scan *s, const char *dir, int depth) {
    if (s->abort) return;
    if (depth > MAX_DEPTH) { s->out->skipped_paths++; return; }
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while (!s->abort && (e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[TRACK_PATH_MAX];
        if (snprintf(p, sizeof p, "%s/%s", dir, e->d_name) >= (int)sizeof p) {
            s->out->skipped_paths++; /* inclui subpastas que nao podem ser percorridas */
            continue;
        }
        struct stat st;
        if (stat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { walk(s, p, depth + 1); continue; }
        if (!is_audio(e->d_name)) continue;

        u_copy(s->key.path, sizeof s->key.path, p);
        const Track *o = s->nold ? bsearch(&s->key, s->old, s->nold, sizeof(Track), cmp_path) : NULL;
        if (o && !s->out->needs_upgrade && o->mp3.sample_rate && o->size == (uint32_t)st.st_size && o->mtime == (int64_t)st.st_mtime) s->tmp = *o;
        else {
            if (build_track(p, &st, &s->tmp) != 0) continue;
            /* Tags ou tamanho mudaram, mas o historico pertence ao caminho da faixa. */
            if (o) {
                s->tmp.added = o->added;
                s->tmp.plays = o->plays;
                s->tmp.last_played = o->last_played;
            }
            s->parsed++;
        }
        if (push(s->out, &s->tmp) != 0) { s->abort = 1; break; }
        if (s->cb && s->cb(s->out->count, p, s->ud)) s->abort = 1;
    }
    closedir(d);
}

int library_scan(Library *l, const char *root, ScanProgress cb, void *ud) {
    Library fresh = {0};
    Track *old = l->tracks; int nold = l->count;
    if (nold > 1) qsort(old, nold, sizeof(Track), cmp_path);
    Scan s;                       /* uma unica instancia (~1 KB); a recursao so recebe o ponteiro */
    memset(&s, 0, sizeof s);
    fresh.needs_upgrade = l->needs_upgrade;
    s.out = &fresh; s.old = old; s.nold = nold; s.cb = cb; s.ud = ud;
    walk(&s, root, 0);
    library_free(l);
    *l = fresh;
    l->needs_upgrade = 0;
    l->last_parsed = s.parsed;
    library_sort(l);
    return s.abort ? 1 : 0;
}
