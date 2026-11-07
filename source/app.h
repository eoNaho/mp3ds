#pragma once
#include "library.h"
#include "queue.h"
#include "playlist.h"
#include "config.h"

typedef enum { TAB_SONGS = 0, TAB_FILA, TAB_ALBUMS, TAB_PLAYLISTS, TAB_N } Tab;

typedef struct {
    Library lib;
    Queue q;
    AlbumGroup *albums; int nalb;            /* reconstruido apos scan/filtro */
    PlaylistInfo pls[MAX_PLAYLISTS]; int npl;
    int index_loaded;                         /* indice persistente valido foi carregado */
    Tab tab;
    int sel[TAB_N], top[TAB_N];              /* selecao e 1a linha visivel, por aba */
    /* busca em TAB_SONGS: linhas sob filtro */
    int *filt; int nfilt;                    /* ids de faixa; NULL = lista inteira */
    char query[128];
    SearchField search_field;
    SearchPlays search_plays;
    int *view_ids; int nview;                /* ordem visivel da aba Musicas */
    SortMode sort_mode;
    int cur;                                 /* indice da faixa tocando (em lib.tracks) ou -1 */
    int fail_streak;
    int screen_off, quit, want_rescan;
    char toast[64]; int toast_frames;
    /* retomada da ultima faixa/posicao */
    char resume_path[TRACK_PATH_MAX]; uint32_t resume_ms; int resume_used;
    unsigned save_count;                     /* contagem para o auto-save do estado */
    int session_dirty;
    int stats_dirty;                         /* plays/last_played ainda nao gravados */
    /* scan em thread (3DS): a UI nao toca em lib enquanto scan_busy */
    volatile int scan_busy, scan_found, scan_abort;
} App;

void app_init(App *a);
void app_exit(App *a);
void app_toast(App *a, const char *msg);
void app_refresh_playlists(App *a);
int  app_needs_initial_scan(const App *a);
int  app_rescan(App *a, ScanProgress cb, void *ud);     /* sincronica: usado no 1o scan no arranque */
void app_start_rescan(App *a);                          /* 3DS: scan em thread (UI continua viva); scan_busy=1 */

void app_tick(App *a);                                  /* 1x por frame: fila, retomada e auto-save */

/* filtragem de linhas de TAB_SONGS (busca): linha <-> faixa */
int  app_songs_rows(const App *a);
int  app_row_to_id(const App *a, int row);              /* -1 se fora do filtro */
int  app_id_to_row(const App *a, int id);               /* -1 se filtrado */
int app_filter_active(const App *a);
void app_set_filter(App *a, const char *query);         /* "" limpa o filtro */
void app_set_sort(App *a, SortMode mode);
const char *app_sort_name(SortMode mode);
void app_cycle_sort(App *a);

/* TAB_ALBUMS: lista de albuns (artista + album) */
void app_play_album(App *a, int group);

/* TAB_FILA: gerenciar a fila por slot (posicao de reproducao) */
int  app_queue_rows(const App *a);
int  app_queue_slot_id(const App *a, int slot);
void app_enqueue(App *a, int id, int next);       /* next: tocar a seguir; senao ao fim */
void app_queue_play_from(App *a, int slot);        /* toca a partir do slot */
int  app_queue_remove(App *a, int slot);           /* remove; devolve o slot atual> */
int  app_queue_move(App *a, int slot, int dir);    /* move ±1; devolve o novo slot */

/* toca a partir de id; sob filtro a fila = resultados da busca, senao a biblioteca inteira */
void app_play_from(App *a, int id);

void app_play_library_from(App *a, int idx);            /* fila = biblioteca inteira, comecando em idx */
void app_open_playlist(App *a, int i);
void app_save_queue_playlist(App *a);
void app_next(App *a, int manual);
void app_prev(App *a);
void app_toggle_pause(App *a);
void app_seek_rel(App *a, int ms);
void app_toggle_shuffle(App *a);
void app_cycle_repeat(App *a);
void app_save_resume(App *a);                           /* grava faixa atual + posicao em RESUME_PATH */
