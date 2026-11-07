#include "app.h"
#include "player.h"
#include "resume.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __3DS__
#include <3ds.h>
#endif

/*生活质量 do estado salvo: quanto tempo entre auto-saves durante a reproducao (frames ~30fps) */
#define SAVE_PERIOD 900u   /* ~30 s */

void app_init(App *a) {
    memset(a, 0, sizeof *a);
    a->cur = -1;
    queue_init(&a->q, (unsigned)time(NULL));
    library_load_index(&a->lib, INDEX_PATH);
    resume_load(RESUME_PATH, a->resume_path, sizeof a->resume_path, &a->resume_ms);
    app_build_albums(a);
    app_refresh_playlists(a);
}

void app_exit(App *a) {
    app_save_resume(a);
    player_stop();
    app_free_scan_old(a);
    queue_free(&a->q);
    library_free(&a->lib);
    free(a->albums); a->albums = NULL; a->nalb = 0;
}#include "app.h"
#include "player.h"
#include "resume.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __3DS__
#include <3ds.h>
#endif

/* qualidade de vida: auto-save do estado de retomada durante a reproducao (frames ~30 fps) */
#define SAVE_PERIOD 900u   /* ~30 s */

static void app_build_albums(App *a) {
    free(a->albums); a->albums = NULL;
    a->nalb = 0;
    if (a->lib.count <= 0) return;
    a->albums = malloc(sizeof(AlbumGroup) * (size_t)a->lib.count);
    if (a->albums) a->nalb = library_build_albums(&a->lib, a->albums, a->lib.count);
}

void app_init(App *a) {
    memset(a, 0, sizeof *a);
    a->cur = -1;
    queue_init(&a->q, (unsigned)time(NULL));
    library_load_index(&a->lib, INDEX_PATH);
    resume_load(RESUME_PATH, a->resume_path, sizeof a->resume_path, &a->resume_ms);
    app_build_albums(a);
    app_refresh_playlists(a);
}

void app_exit(App *a) {
    app_save_resume(a);
    player_stop();
    app_finish_scan_old(a);   /* libera a biblioteca antiga pendente (scan em thread) */
    queue_free(&a->q);
    library_free(&a->lib);
    free(a->albums); a->albums = NULL; a->nalb = 0;
    resume_save(RESUME_PATH, "", 0);   /* estado persistido; limpa o arquivo */
    resume_free_queue(a);
}

/* ---------- eventos de fila e busca ---------- */

int app_songs_rows(const App *a) { return a->filt ? a->nfilt : a->lib.count; }

int app_row_to_id(const App *a, int row) {
    int n = app_songs_rows(a);
    if (row < 0 || row >= n) return -1;
    return a->filt ? a->filt[row] : row;
}

int app_id_to_row(const App *a, int id) {
    if (id < 0 || id >= a->lib.count) return -1;
    if (!a->filt) return id;
    for (int i = 0; i < a->nfilt; i++) if (a->filt[i] == id) return i;
    return -1;
}

void app_set_filter(App *a, const char *query) {
    free(a->filt); a->filt = NULL; a->nfilt = 0;
    u_copy(a->query, sizeof a->query, query ? query : "");
    if (!a->query[0]) {
        int t = a->tab;
        if (a->sel[t] >= a->lib.count) a->sel[t] = a->lib.count ? a->lib.count - 1 : 0;
        a->top[t] = 0;
        return;
    }
    if (a->lib.count <= 0) return;
    a->filt = malloc(sizeof(int) * (size_t)a->lib.count);
    if (!a->filt) return;
    a->nfilt = library_search(&a->lib, a->query, a->filt, a->lib.count);
    if (a->settled && a->tab == TAB_SONGS) a->sel[TAB_SONGS] = 0, a->top[TAB_SONGS] = 0;
}

static void filter_row(App *a) {
    if (a->nfilt == 0) return;
    int t = a->tab;
    if (a->sel[t] >= a->nfilt) a->sel[t] = a->nfilt - 1;
    if (a->sel[t] < 0) a->sel[t] = 0;
}

/* ---------- playlists ---------- */

void app_toast(App *a, const char *msg) { u_copy(a->toast, sizeof a->toast, msg); a->toast_frames = 120; }

void app_refresh_playlists(App *a) { a->npl = playlist_scan(PLAYLIST_DIR, a->pls, MAX_PLAYLISTS); }

int app_rescan(App *a, ScanProgress cb, void *ud) {
    player_stop();
    queue_free(&a->q); queue_init(&a->q, (unsigned)time(NULL));
    a->cur = -1; a->settled = 0;
    int rc = library_scan(&a->lib, MUSIC_ROOT, cb, ud);
    library_save_index(&a->lib, INDEX_PATH);
    app_set_filter(a, a->query);       /* re-executa a busca atual, se houver */
    return rc;
}

void app_save_queue_playlist(App *a) {
    if (!a->settled) { app_toast(a, "Aguarde o scan terminar"); return; }
    if (a->q.count == 0) { app_toast(a, "A fila esta vazia"); return; }
    char (*paths)[PL_PATH_MAX] = malloc((size_t)PL_PATH_MAX * a->q.count);
    if (!paths) return;
    for (int i = 0; i < a->q.count; i++) u_copy(paths[i], PL_PATH_MAX, a->lib.tracks[a->q.items[i]].path);
    char file[300];
    for (int n = 1; n < 1000; n++) {
        snprintf(file, sizeof file, PLAYLIST_DIR "/Fila %02d.m3u", n);
        FILE *f = fopen(file, "rb");
        if (f) { fclose(f); continue; }
        if (playlist_save(file, (const char (*)[PL_PATH_MAX])paths, a->q.count) == 0) { app_toast(a, "Fila salva como playlist"); app_refresh_playlists(a); }
        else app_toast(a, "Erro ao salvar playlist");
        break;
    }
    free(paths);
}

/* ---------- albuns ---------- */

void app_play_album(App *a, int g) {
    if (g < 0 || g >= a->nalb) return;
    const AlbumGroup *al = &a->albums[g];
    int n = al->count;
    int *ids = malloc(sizeof(int) * (size_t)n);
    if (!ids) return;
    for (int i = 0; i < n; i++) ids[i] = al->first + i;
    play_ids(a, ids, n, 0);
    free(ids);
}

/* ---------- retomada ---------- */

void app_save_resume(App *a) {
    if (a->cur >= 0 && a->cur < a->lib.count && player_state() != PS_STOPPED) {
        uint32_t ms = player_pos_ms();
        a->resume_ms = ms;
        u_copy(a->resume_path, sizeof a->resume_path, a->lib.tracks[a->cur].path);
        resume_save(RESUME_PATH, a->resume_path, ms);
    } else {
        a->resume_ms = 0;
    }
}

void app_play_resume(App *a) {
    if (!a->resume_path[0]) return;
    int id = library_find_path(&a->lib, a->resume_path);
    if (id < 0) return;
    app_play_library_from(a, id);
#ifdef __3DS__
    int ms = a->resume_ms;
    resume_save(RESUME_PATH, a->resume_path, ms);
#endif
}

/* ---------- fila / reproducao ---------- */

void app_next(App *a, int manual) {
    if (a->settled == 0) return;
    if (a->q.count == 0) { app_toast(a, "A fila esta vazia"); return; }
    int id = queue_next(&a->q, manual);
    if (id < 0) { if (!manual) { player_stop(); a->cur = -1; } return; }
    start_track(a, id);
}

void app_prev(App *a) {
    if (a->settled == 0) return;
    if (player_pos_ms() > 3000) { player_seek(0); return; }   /* 1o toque reinicia, 2o volta */
    if (a->q.count == 0) return;
    int id = queue_prev(&a->q);
    if (id >= 0) start_track(a, id);
}

void app_toggle_pause(App *a) {
    if (a->settled == 0) return;
    PlayerState s = player_state();
    if (s == PS_PLAYING) player_pause(1);
    else if (s == PS_PAUSED) player_pause(0);
    else if (a->cur >= 0) start_track(a, a->cur);
    else if (a->tab == TAB_SONGS && a->lib.count) app_play_library_from(a, a->sel[TAB_SONGS]);
}

void app_seek_rel(App *a, int ms) {
    if (a->settled == 0) return;
    if (player_state() == PS_STOPPED) return;
    int64_t t = (int64_t)player_pos_ms() + ms, d = player_duration_ms();
    if (t < 0) t = 0;
    if (d > 0 && t >= d - 500) t = d - 500 > 0 ? d - 500 : 0;
    player_seek((uint32_t)t);
}

/* ---------- equalizador ---------- */

void app_cycle_eq(App *a) {
    player_eq_cycle();
    char t[64];
    snprintf(t, sizeof t, "EQ: %s", player_eq_name());
    app_toast(a, t);
}

void app_cycle_repeat(App *a) {
    a->q.repeat = (RepeatMode)((a->q.repeat + 1) % 3);
    app_toast(a, a->q.repeat == REPEAT_OFF ? "Repetir: desligado" : a->q.repeat == REPEAT_ALL ? "Repetir: tudo" : "Repetir: uma faixa");
}

void app_toggle_shuffle(App *a) {
    queue_set_shuffle(&a->q, !a->q.shuffle);
    app_toast(a, a->q.shuffle ? "Aleatorio: ligado" : "Aleatorio: desligado");
}

/* ---------- tick por frame ---------- */

void app_tick(App *a) {
    player_take_finished();
    if (a->settled == 0) return;
    if (player_state() == PS_STOPPED) { a->cur = -1; a->resume_ms = 0; }
    else if (a->cur >= 0) a->resume_ms = player_pos_ms();
    if ((++a->auto_save % 900) == 0) app_save_resume(a);   /* ~30 s */
}
