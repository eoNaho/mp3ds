#include "app.h"
#include "player.h"
#include "resume.h"
#include "session.h"
#include "persistence.h"
#include "util.h"
#include "cover.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#ifdef __3DS__
#include <3ds.h>
#endif

/* auto-save do estado de retomada durante a reproducao (frames ~30 fps) */
#define SAVE_PERIOD 900u   /* ~30 s */

void app_toast(App *a, const char *msg) { u_copy(a->toast, sizeof a->toast, msg); a->toast_frames = 120; }

static int playlist_name_exists(const PlaylistInfo *list, int n, const char *name) {
    for (int i = 0; i < n; i++) if (!strcasecmp(list[i].name, name)) return 1;
    return 0;
}

static int playlist_cmp(const void *pa, const void *pb) {
    return strcasecmp(((const PlaylistInfo *)pa)->name, ((const PlaylistInfo *)pb)->name);
}

void app_refresh_playlists(App *a) {
    PlaylistInfo *found = malloc(sizeof(PlaylistInfo) * MAX_PLAYLISTS);
    PlaylistInfo *extra = malloc(sizeof(PlaylistInfo) * MAX_PLAYLISTS);
    if (!found || !extra) { free(found); free(extra); return; }
    /* Prefer the copy next to the music. An older copy in DATA_DIR must not
       hide the playlist that was just copied to Music/Playlists. */
    int n = playlist_scan(MUSIC_ROOT, found, MAX_PLAYLISTS);
    int m = playlist_scan(MUSIC_PLAYLIST_DIR, extra, MAX_PLAYLISTS);
    for (int i = 0; i < m && n < MAX_PLAYLISTS; i++)
        if (!playlist_name_exists(found, n, extra[i].name)) found[n++] = extra[i];

    m = playlist_scan(PLAYLIST_DIR, extra, MAX_PLAYLISTS);
    for (int i = 0; i < m && n < MAX_PLAYLISTS; i++)
        if (!playlist_name_exists(found, n, extra[i].name)) found[n++] = extra[i];

    qsort(found, n, sizeof *found, playlist_cmp);
    memcpy(a->pls, found, sizeof(PlaylistInfo) * (size_t)n);
    a->npl = n;
    free(found); free(extra);
}

int app_needs_initial_scan(const App *a) {
    if (!a->index_loaded || a->lib.needs_upgrade) return 1;

    /* Um índice antigo pode continuar exibindo as faixas depois que elas
       foram reorganizadas no SD. Nesse caso as músicas aparecem na tela, mas
       o caminho salvo não coincide com o caminho das playlists. Faz uma única
       sincronização para atualizar esses caminhos. */
    for (int i = 0; i < a->lib.count; i++) {
        struct stat st;
        if (stat(a->lib.tracks[i].path, &st) != 0) return 1;
    }
    return 0;
}

static void app_play_ids(App *a, const int *ids, int n, int start);

/* ---------------- albums (TAB_ALBUMS) ---------------- */

static void app_build_albums(App *a) {
    free(a->albums); a->albums = NULL;
    a->nalb = 0;
    if (a->lib.count <= 0) return;
    a->albums = malloc(sizeof(AlbumGroup) * (size_t)a->lib.count);
    if (a->albums) a->nalb = library_build_albums(&a->lib, a->albums, a->lib.count);
}

void app_play_album(App *a, int g) {
    if (a->scan_busy || g < 0 || g >= a->nalb) return;
    const AlbumGroup *al = &a->albums[g];
    /* o grupo cobre as faixas [first, first+count); a fila usa a propria biblioteca nessa sequencia */
    int n = al->count;
    int *ids = malloc(sizeof(int) * (size_t)(n > 0 ? n : 1));
    if (!ids) return;
    for (int i = 0; i < n; i++) ids[i] = al->first + i;
    app_play_ids(a, ids, n, 0);
    free(ids);
}

/* ---------------- busca (TAB_SONGS) ---------------- */

static void rebuild_view(App *a) {
    free(a->view_ids); a->view_ids = NULL; a->nview = 0;
    if (a->lib.count <= 0) return;
    a->view_ids = malloc(sizeof(int) * (size_t)a->lib.count);
    if (!a->view_ids) return;
    if (app_filter_active(a)) {
        /* Uma busca sem resultados mantem filt == NULL. Nesse caso a lista
           visivel precisa continuar vazia, e nao voltar a mostrar a
           biblioteca inteira. */
        if (a->filt && a->nfilt > 0) {
            memcpy(a->view_ids, a->filt, sizeof(int) * (size_t)a->nfilt);
            a->nview = a->nfilt;
        }
    } else {
        for (int i = 0; i < a->lib.count; i++) a->view_ids[i] = i;
        a->nview = a->lib.count;
    }
    library_sort_ids(&a->lib, a->view_ids, a->nview, a->sort_mode);
}

int app_songs_rows(const App *a) { return a->nview; }

int app_row_to_id(const App *a, int row) {
    int n = app_songs_rows(a);
    if (row < 0 || row >= n) return -1;
    return a->view_ids ? a->view_ids[row] : -1;
}

int app_id_to_row(const App *a, int id) {
    if (id < 0 || id >= a->lib.count) return -1;
    for (int i = 0; i < a->nview; i++) if (a->view_ids[i] == id) return i;
    return -1;
}

int app_filter_active(const App *a) {
    return a->query[0] || a->search_field != SEARCH_ALL || a->search_plays != SEARCH_ANY_PLAY;
}

void app_set_filter(App *a, const char *query) {
    free(a->filt); a->filt = NULL; a->nfilt = 0;
    if (query != a->query) u_copy(a->query, sizeof a->query, query ? query : "");
    if (app_filter_active(a) && a->lib.count > 0) {
        a->filt = malloc(sizeof(int) * (size_t)a->lib.count);
        if (!a->filt) { a->query[0] = 0; a->search_field = SEARCH_ALL; a->search_plays = SEARCH_ANY_PLAY; app_toast(a, "Sem memoria para filtrar"); }
        else {
            a->nfilt = library_search_filtered(&a->lib, a->query, a->search_field, a->search_plays, a->filt, a->lib.count);
            if (a->nfilt == 0) { free(a->filt); a->filt = NULL; }
        }
    }
    rebuild_view(a);
    a->sel[a->tab] = a->top[a->tab] = 0;
}

const char *app_sort_name(SortMode mode) {
    switch (mode) {
        case SORT_ALBUM: return "Album";
        case SORT_TITULO: return "Titulo";
        case SORT_ADICAO: return "Adicao";
        case SORT_RECENTES: return "Recentes";
        default: return "Artista";
    }
}

void app_set_sort(App *a, SortMode mode) {
    if (mode < SORT_ARTISTA || mode > SORT_RECENTES) mode = SORT_ARTISTA;
    int old_id = app_row_to_id(a, a->sel[TAB_SONGS]);
    a->sort_mode = mode;
    rebuild_view(a);
    int row = app_id_to_row(a, old_id);
    a->sel[TAB_SONGS] = row >= 0 ? row : 0;
    a->top[TAB_SONGS] = 0;
}

void app_cycle_sort(App *a) {
    app_set_sort(a, (SortMode)((a->sort_mode + 1) % (SORT_RECENTES + 1)));
    char msg[64]; snprintf(msg, sizeof msg, "Ordenar: %s", app_sort_name(a->sort_mode));
    app_toast(a, msg);
}

/* ---------------- playlists ---------------- */

void app_open_playlist(App *a, int i) {
    if (a->scan_busy || i < 0 || i >= a->npl) return;
    char (*paths)[PL_PATH_MAX] = malloc((size_t)PL_PATH_MAX * 512);
    int *ids = malloc(sizeof(int) * 512);
    if (!paths || !ids) { free(paths); free(ids); return; }
    int n = playlist_load(a->pls[i].path, paths, 512), m = 0;
    if (n < 0) {
        app_toast(a, "Erro ao abrir playlist");
        free(paths); free(ids);
        return;
    }
    for (int k = 0; k < n; k++) { int id = library_find_path(&a->lib, paths[k]); if (id >= 0) ids[m++] = id; }
    if (m == 0) app_toast(a, "Playlist vazia ou faixas nao encontradas");
    else { char name[40]; u_utf8_trunc(name, sizeof name, a->pls[i].name, 20); char t[64]; snprintf(t, sizeof t, "%s: %d faixas", name, m); app_toast(a, t); app_play_ids(a, ids, m, 0); }
    free(paths); free(ids);
}

void app_save_queue_playlist(App *a) {
    if (a->scan_busy) { app_toast(a, "Aguarde o scan terminar"); return; }
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

/* ---------------- reproducao ---------------- */

static void start_track(App *a, int id) {
    if (id < 0 || id >= a->lib.count) { player_stop(); a->cur = -1; return; }
    a->cur = id;
    a->session_dirty = 1;
    const char *p = a->lib.tracks[id].path;
    /* 1a reproducao da sessao: se for a ultima faixa, retoma do ponto salvo */
    if (!a->resume_used && a->resume_ms > 2000 && !strcmp(p, a->resume_path)) {
        player_play_track(&a->lib.tracks[id], a->resume_ms);
        a->resume_used = 1;
    } else {
        player_play_track(&a->lib.tracks[id], 0);
    }
    library_record_play(&a->lib, id, 0);
    a->stats_dirty = 1;
    if (a->sort_mode == SORT_RECENTES) {
        library_sort_ids(&a->lib, a->view_ids, a->nview, a->sort_mode);
        int row = app_id_to_row(a, id);
        if (row >= 0) a->sel[TAB_SONGS] = row;
    }
}

static void app_play_ids(App *a, const int *ids, int n, int start) {
    if (n <= 0) return;
    if (queue_set(&a->q, ids, n, start) != 0) {
        app_toast(a, "Sem memoria para montar a fila"); return;
    }
    start_track(a, queue_current(&a->q));
}

void app_play_library_from(App *a, int idx) {
    if (a->scan_busy) return;
    int n = a->lib.count;
    if (idx < 0 || idx >= n) return;
    int *ids = malloc(sizeof(int) * n);
    if (!ids) return;
    for (int i = 0; i < n; i++) ids[i] = i;
    app_play_ids(a, ids, n, idx);
    free(ids);
}

void app_play_from(App *a, int id) {
    if (a->scan_busy) return;
    if (app_filter_active(a)) {
        if (!a->filt || a->nfilt <= 0) return;
        int row = app_id_to_row(a, id);
        if (row >= 0) { app_play_ids(a, a->view_ids, a->nview, row); return; }
    }
    app_play_library_from(a, id);
}

/* ---------------- fila (aba Fila) ---------------- */

int app_queue_rows(const App *a) { return a->q.count; }

int app_queue_slot_id(const App *a, int slot) { return queue_slot_id(&a->q, slot); }

void app_queue_play_from(App *a, int slot) {
    if (a->scan_busy || a->q.count == 0) return;
    queue_set_pos(&a->q, slot);
    start_track(a, queue_current(&a->q));
}

int app_queue_remove(App *a, int slot) {
    if (a->scan_busy || a->q.count == 0) return queue_current_slot(&a->q);
    a->session_dirty = 1;
    int cur = queue_remove_slot(&a->q, slot);
    if (a->q.count == 0) { player_stop(); a->cur = -1; app_save_resume(a); }
    return cur;
}

int app_queue_move(App *a, int slot, int dir) {
    if (a->scan_busy || a->q.count == 0) return queue_current_slot(&a->q);
    a->session_dirty = 1;
    return queue_move_slot(&a->q, slot, dir);
}

void app_next(App *a, int manual) {
    int id = queue_next(&a->q, manual);
    if (id < 0) {
        if (!manual) {
            player_stop(); a->cur = -1; app_save_resume(a);
        }
        return;
    }
    start_track(a, id);
}

void app_prev(App *a) {
    if (player_pos_ms() > 3000) { player_seek(0); return; }   /* como em qualquer player: 1o toque reinicia */
    int id = queue_prev(&a->q);
    if (id >= 0) start_track(a, id);
}

void app_enqueue(App *a, int id, int next) {
    if (a->scan_busy || id < 0 || id >= a->lib.count) return;
    if (queue_insert(&a->q, id, next) < 0) {
        app_toast(a, "Sem memoria para adicionar a faixa"); return;
    }
    a->session_dirty = 1;
    app_save_resume(a);
    app_toast(a, next ? "Adicionada para tocar a seguir" : "Adicionada ao fim da fila");
}

void app_toggle_pause(App *a) {
    PlayerState s = player_state();
    if (s == PS_PLAYING) { player_pause(1); app_save_resume(a); }
    else if (s == PS_PAUSED) player_pause(0);
    else if (a->q.count) start_track(a, queue_current(&a->q));
    else if (a->cur >= 0) app_play_library_from(a, a->cur);
    else if (a->tab == TAB_SONGS && app_songs_rows(a)) app_play_from(a, app_row_to_id(a, a->sel[a->tab]));
}

void app_seek_rel(App *a, int ms) {
    (void)a;
    if (player_state() == PS_STOPPED) return;
    int64_t t = (int64_t)player_pos_ms() + ms, d = player_duration_ms();
    if (t < 0) t = 0;
    if (d > 0 && t >= d - 500) t = d - 500 > 0 ? d - 500 : 0;
    player_seek((uint32_t)t);
}

void app_toggle_shuffle(App *a) { a->session_dirty = 1; queue_set_shuffle(&a->q, !a->q.shuffle); app_toast(a, a->q.shuffle ? "Aleatorio: ligado" : "Aleatorio: desligado"); }

void app_cycle_repeat(App *a) {
    a->session_dirty = 1;
    a->q.repeat = (RepeatMode)((a->q.repeat + 1) % 3);
    app_toast(a, a->q.repeat == REPEAT_OFF ? "Repetir: desligado" : a->q.repeat == REPEAT_ALL ? "Repetir: tudo" : "Repetir: uma faixa");
}

/* ---------------- retomada ---------------- */

/* salva faixa atual + posicao; sem faixa tocando, apaga o estado */
void app_capture_resume(App *a) {
    if (a->scan_busy) return;
    if (a->cur >= 0 && a->cur < a->lib.count && player_state() != PS_STOPPED) {
        u_copy(a->resume_path, sizeof a->resume_path, a->lib.tracks[a->cur].path);
        a->resume_ms = player_pos_ms();
    } else if (a->cur < 0) {
        a->resume_path[0] = 0; a->resume_ms = 0;
    }
}

void app_save_resume(App *a) {
    if (a->scan_busy) return;
    app_capture_resume(a);
    if (a->save_retry_frames && !a->quit) return;
    if (persistence_submit_parts(&a->lib, &a->q, a->resume_path, a->resume_ms, a->session_dirty, a->stats_dirty) == 0) {
        a->stats_dirty = a->session_dirty = 0;
    }
    else { a->stats_dirty = a->session_dirty = 1; app_toast(a, "Nao consegui agendar o salvamento"); }
}

/* ---------------- scan (sincronico para o arranque; thread durante uso) ---------------- */

int app_rescan(App *a, ScanProgress cb, void *ud) {
    player_stop();
    a->session_dirty = 1;
    queue_free(&a->q); queue_init(&a->q, (unsigned)time(NULL));
    a->cur = -1;
    cover_clear_missing(COVER_DIR);
    int rc = library_scan(&a->lib, MUSIC_ROOT, cb, ud);
    library_save_index(&a->lib, INDEX_PATH);
    app_build_albums(a);
    app_set_filter(a, a->query);        /* re-executa o filtro atual se houver */
    rebuild_view(a);
    if (a->lib.skipped_paths) {
        char b[64]; snprintf(b, sizeof b, "Scan: %d caminhos longos/profundos ignorados", a->lib.skipped_paths); app_toast(a, b);
    }
    return rc;
}

#ifdef __3DS__

static Library s_fresh;                 /* biblioteca nova construida pelo worker; trocada no tick (contexto da UI) */
static volatile int s_scan_done;
static Thread s_scan_thread;

static void join_scan(void) {
    if (s_scan_thread) {
        threadJoin(s_scan_thread, U64_MAX);
        threadFree(s_scan_thread);
        s_scan_thread = NULL;
    }
}

static int scan_cb(int found, const char *cur, void *ud) {
    (void)cur;
    App *a = ud;
    a->scan_found = found;
    return a->scan_abort ? 1 : 0;
}

static void scan_worker(void *arg) {
    App *a = arg;
    osSetSpeedupEnable(true);            /* clock cheia enquanto decodifica tags de faixas novas */
    cover_clear_missing(COVER_DIR);
    library_scan(&s_fresh, MUSIC_ROOT, scan_cb, a);
    osSetSpeedupEnable(false);
    library_save_index(&s_fresh, INDEX_PATH);
    s_scan_done = 1;                     /* app_tick faz a troca; daqui a UI nunca corre com lib rastrada */
    threadExit(0);
}

void app_start_rescan(App *a) {
    if (a->scan_busy) return;
    if (persistence_busy()) { a->want_rescan = 1; return; }
    a->scan_abort = 0;
    a->scan_found = 0;
    player_stop();
    a->session_dirty = 1;
    queue_free(&a->q); queue_init(&a->q, (unsigned)time(NULL));
    a->cur = -1;
    s32 prio = 0x30; svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    memset(&s_fresh, 0, sizeof s_fresh);
    s_fresh.needs_upgrade = a->lib.needs_upgrade;
    if (a->lib.count) {
        s_fresh.tracks = malloc(sizeof(Track) * (size_t)a->lib.count);
        if (!s_fresh.tracks) { app_toast(a, "Sem memoria para reescanear"); return; }
        memcpy(s_fresh.tracks, a->lib.tracks, sizeof(Track) * (size_t)a->lib.count);
        s_fresh.count = s_fresh.cap = a->lib.count;
    }
    s_scan_thread = threadCreate(scan_worker, a, 96 * 1024, prio + 1, -2, false);
    if (!s_scan_thread) { library_free(&s_fresh); return; }
    a->scan_busy = 1;
}

#endif /* __3DS__ */

/* ---------------- tick por frame ---------------- */

void app_tick(App *a) {
    if (a->save_retry_frames) a->save_retry_frames--;
    int parts, code;
    if (persistence_take_error_detail(&parts, &code)) {
        a->stats_dirty = a->session_dirty = 1;
        a->save_retry_frames = 150; /* ~5 s: nao repete I/O e capturas a cada frame */
        const char *file = parts & SAVE_SESSION_ERROR ? "session.txt" :
                           parts & SAVE_RESUME_ERROR ? "resume.txt" :
                           parts & SAVE_INDEX_ERROR ? "library.idx" : "pasta de dados";
        char message[64]; snprintf(message, sizeof message, "SD: %s (erro %d)", file, code);
        app_toast(a, message);
    }
    int f = player_take_finished();
    if (f == 2) {
        int started = player_take_started();
        int next = queue_next(&a->q, 0);
        if (started >= 0 && started < a->lib.count) {
            a->cur = started; a->session_dirty = 1;
            if (next != started) {
                for (int slot = 0; slot < a->q.count; slot++)
                    if (queue_slot_id(&a->q, slot) == started) { queue_set_pos(&a->q, slot); break; }
            }
            library_record_play(&a->lib, started, 0); a->stats_dirty = 1;
            if (a->sort_mode == SORT_RECENTES) library_sort_ids(&a->lib, a->view_ids, a->nview, a->sort_mode);
        }
    }
    else if (f == 1) { a->fail_streak = 0; app_next(a, 0); }
    else if (f == -1) {                      /* arquivo ruim: pula, mas nao entra em loop se tudo falhar */
        if (++a->fail_streak < 5) app_next(a, 0); else { player_stop(); a->cur = -1; app_save_resume(a); app_toast(a, "Nao consegui tocar as faixas"); }
    }
    if (a->toast_frames > 0) a->toast_frames--;

    /* Scan terminou: troca biblioteca e caches juntos, antes do proximo desenho. */
#ifdef __3DS__
    if (a->scan_busy && s_scan_done) {
        join_scan();
        s_scan_done = 0;
        library_free(&a->lib);
        a->lib = s_fresh;
        memset(&s_fresh, 0, sizeof s_fresh);
        a->scan_busy = 0;
        app_build_albums(a);
        app_set_filter(a, a->query);
        if (a->lib.skipped_paths) {
            char b[64]; snprintf(b, sizeof b, "Scan: %d caminhos longos/profundos ignorados", a->lib.skipped_paths); app_toast(a, b);
        } else if (!a->lib.count) app_toast(a, "Scan: nenhuma faixa encontrada");
        else { char b[64]; snprintf(b, sizeof b, "Biblioteca: %d faixas", a->lib.count); app_toast(a, b); }
    }
#endif

    if (!a->scan_busy && player_state() != PS_STOPPED) {
        int next = queue_peek_next(&a->q);
        player_prepare_next(next >= 0 && next < a->lib.count ? &a->lib.tracks[next] : NULL, next);
    }

    /* retomada: guarda a posicao continuamente; auto-save periodico durante a reproducao */
    if (a->cur >= 0 && player_state() != PS_STOPPED) {
        a->resume_ms = player_pos_ms();
        u_copy(a->resume_path, sizeof a->resume_path, a->lib.tracks[a->cur].path);
        if (++a->save_count >= SAVE_PERIOD && !a->save_retry_frames) {
            a->save_count = 0;
            /* O tick grava apenas a posicao: nao copia biblioteca nem fila. */
            if (persistence_submit_parts(NULL, NULL, a->resume_path, a->resume_ms, 0, 0) != 0)
                app_toast(a, "Nao consegui agendar o salvamento");
        }
    } else {
        a->save_count = 0;
    }
    /* A sessao so muda com fila, posicao, shuffle ou repeat. Estatisticas sao
       gravadas ao pausar/sair; o tick periodico acima permanece resume-only. */
    if (a->session_dirty && !a->scan_busy && !a->save_retry_frames && !persistence_busy()) {
        if (persistence_submit_parts(&a->lib, &a->q, a->resume_path, a->resume_ms, 1, 0) == 0)
            a->session_dirty = 0;
    }

}

/* ---------------- ciclo de vida ---------------- */

void app_init(App *a) {
    memset(a, 0, sizeof *a);
    a->cur = -1;
    if (persistence_init() != 0) app_toast(a, "Salvamento em segundo plano indisponivel");
    queue_init(&a->q, (unsigned)time(NULL));
    a->sort_mode = SORT_ARTISTA;
    a->index_loaded = library_load_index(&a->lib, INDEX_PATH) == 0;
    resume_load(RESUME_PATH, a->resume_path, sizeof a->resume_path, &a->resume_ms);
    app_build_albums(a);
    app_refresh_playlists(a);
    rebuild_view(a);
    if (session_load(SESSION_PATH, &a->lib, &a->q) == 0) {
        a->cur = queue_current(&a->q);
        app_toast(a, "Fila restaurada. B ou play para continuar");
    }
    /* retomada: seleciona a ultima faixa tocada para achar facil (e mostra no topo) */
    int i = library_find_path(&a->lib, a->resume_path);
    if (i >= 0) { a->sel[TAB_SONGS] = app_id_to_row(a, i); a->top[TAB_SONGS] = a->sel[TAB_SONGS] > 0 ? a->sel[TAB_SONGS] - 1 : 0; if (!a->q.count) a->cur = i; }
}

void app_exit(App *a) {
    a->quit = 1;
#ifdef __3DS__
    /* O scan usa App e o SD: cancelar e aguardar antes de salvar/liberar tudo. */
    a->scan_abort = 1;
    join_scan();
    if (a->scan_busy) {
        library_free(&s_fresh);
        a->scan_busy = 0;
        s_scan_done = 0;
    }
#endif
    app_save_resume(a);
    persistence_exit();
    if (persistence_take_error()) a->stats_dirty = 1;
    /* Fallback apenas ao sair, se o worker nao iniciou ou o SD falhou. */
    if ((a->stats_dirty || a->session_dirty) && !a->scan_busy) {
        SaveState *state = save_state_capture(&a->lib, &a->q, a->resume_path, a->resume_ms, 1);
        if (state) { save_state_write(state, SESSION_PATH, RESUME_PATH, INDEX_PATH); save_state_free(state); }
    }
    player_stop();
    queue_free(&a->q);
    library_free(&a->lib);
    free(a->albums); a->albums = NULL; a->nalb = 0;
    free(a->filt); a->filt = NULL; a->nfilt = 0;
    free(a->view_ids); a->view_ids = NULL; a->nview = 0;
}
