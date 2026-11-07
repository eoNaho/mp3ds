/* Interface 2D com citro2d: so retangulos, gradientes de 4 cantos e texturas pequenas. Sem blur/alpha pesado. */
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"
#include "covers.h"
#include "player.h"
#include "util.h"

#define ROW_H    28
#define ROWS     5
#define LIST_Y   30
#define TOP_W    400
#define BOT_W    320
#define MAX_QUERY 40

static C3D_RenderTarget *s_top, *s_bot;
static C2D_TextBuf s_buf;
static u32 BG, PANEL, TXT, DIM, ACCENT0;

/* estado do toque */
static int t_down, t_sx, t_sy, t_ly, t_moved, t_acc;

static void screens_power(App *a, int off) {
    a->screen_off = off;
    if (off) { app_save_resume(a); GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_BOTH); }
    else GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTH);
}

void ui_init(void) {
    gfxInitDefault();
    gfxSet3D(false);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    C3D_FrameRate(30.0f);
    gspLcdInit();
    s_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    s_bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    s_buf = C2D_TextBufNew(4096);
    BG = C2D_Color32(16, 16, 22, 255); PANEL = C2D_Color32(32, 32, 44, 255);
    TXT = C2D_Color32(240, 240, 245, 255); DIM = C2D_Color32(145, 145, 165, 255);
    ACCENT0 = C2D_Color32(124, 108, 255, 255);
    hidSetRepeatParameters(20, 4);
}

void ui_exit(void) {
    GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTH);
    gspLcdExit();
    C2D_TextBufDelete(s_buf);
    C2D_Fini(); C3D_Fini(); gfxExit();
}

/* ---------- helpers de desenho ---------- */
static void text(const char *s, float x, float y, float scale, u32 clr) {
    C2D_Text t;
    C2D_TextParse(&t, s_buf, s);
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.6f, scale, scale, clr);
}
static void text_center(const char *s, float cx, float y, float scale, u32 clr) {
    C2D_Text t; float w, h;
    C2D_TextParse(&t, s_buf, s);
    C2D_TextOptimize(&t);
    C2D_TextGetDimensions(&t, scale, scale, &w, &h);
    C2D_DrawText(&t, C2D_WithColor, cx - w / 2, y, 0.6f, scale, scale, clr);
}
static void rect(float x, float y, float w, float h, u32 c) { C2D_DrawRectSolid(x, y, 0.5f, w, h, c); }
static void rrect(float x, float y, float w, float h, float r, u32 c) {
    rect(x + r, y, w - 2 * r, h, c); rect(x, y + r, r, h - 2 * r, c); rect(x + w - r, y + r, r, h - 2 * r, c);
    C2D_DrawCircleSolid(x + r, y + r, 0.5f, r, c); C2D_DrawCircleSolid(x + w - r, y + r, 0.5f, r, c);
    C2D_DrawCircleSolid(x + r, y + h - r, 0.5f, r, c); C2D_DrawCircleSolid(x + w - r, y + h - r, 0.5f, r, c);
}
static void tri(float x0, float y0, float x1, float y1, float x2, float y2, u32 c) {
    C2D_DrawTriangle(x0, y0, c, x1, y1, c, x2, y2, c, 0.6f);
}
static void fmt_time(char *o, size_t n, uint32_t ms) { uint32_t s = ms / 1000; snprintf(o, n, "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60)); }

/* ---------- icones (formas simples) ---------- */
static void icon_play(float cx, float cy, float s, u32 c) { tri(cx - s * 0.4f, cy - s * 0.6f, cx - s * 0.4f, cy + s * 0.6f, cx + s * 0.6f, cy, c); }
static void icon_pause(float cx, float cy, float s, u32 c) { rect(cx - s * 0.5f, cy - s * 0.55f, s * 0.35f, s * 1.1f, c); rect(cx + s * 0.15f, cy - s * 0.55f, s * 0.35f, s * 1.1f, c); }
static void icon_next(float cx, float cy, float s, u32 c) { tri(cx - s * 0.6f, cy - s * 0.5f, cx - s * 0.6f, cy + s * 0.5f, cx + s * 0.3f, cy, c); rect(cx + s * 0.35f, cy - s * 0.5f, s * 0.2f, s, c); }
static void icon_prev(float cx, float cy, float s, u32 c) { tri(cx + s * 0.6f, cy - s * 0.5f, cx + s * 0.6f, cy + s * 0.5f, cx - s * 0.3f, cy, c); rect(cx - s * 0.55f, cy - s * 0.5f, s * 0.2f, s, c); }
static void icon_search(float cx, float cy, u32 c) {      /* lupa */
    C2D_DrawCircleSolid(cx, cy, 0.6f, 7, c);
    C2D_DrawCircleSolid(cx, cy, 0.62f, 5, BG);
    tri(cx + 4, cy + 4, cx + 4, cy + 9, cx + 9, cy + 4, c);
}
static void icon_clear(float cx, float cy, u32 c) {       /* X */
    C2D_DrawLine(cx - 6, cy - 6, c, cx + 6, cy + 6, c, 2.0f, 0.62f);
    C2D_DrawLine(cx - 6, cy + 6, c, cx + 6, cy - 6, c, 2.0f, 0.62f);
}

/* resposta aproximada de cada preset EQ (5 barras: 1s do perfil de frequencia) */
static const unsigned char EQ_BARS[6][5] = {
    { 3, 3, 3, 3, 3 },   /* Desligado: flat      */
    {13, 9, 4, 2, 1 },   /* Sub                  */
    {11, 8, 4, 2, 2 },   /* Graves               */
    { 2, 4, 8,11, 6 },   /* Voz                  */
    { 2, 3, 5, 9,13 },   /* Brilho               */
    { 3, 5, 3, 7, 4 },   /* Suave (corte @3 kHz) */
};

/* ---------- busca por teclado (3DS) ---------- */
static void ask_query(App *a) {
    if (a->screen_off) screens_power(a, 0);
    static SwkbdState sw;                                 /* ~4 KB; fora da pilha */
    swkbdInit(&sw, SWKBD_TYPE_WESTERN, 1, MAX_QUERY);
    swkbdSetValidation(&sw, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetHintText(&sw, "titulo, artista ou album");
    swkbdSetInitialText(&sw, a->query);                   /* App persiste durante a chamada */
    char buf[MAX_QUERY + 1];
    if (swkbdInputText(&sw, buf, sizeof buf) == SWKBD_BUTTON_CONFIRM) {
        app_set_filter(a, buf);
        int n = app_songs_rows(a);
        if (n == 0) app_toast(a, "Nada encontrado");
        else { char t[64]; snprintf(t, sizeof t, "\"%s\": %d faixa(s)", buf, n); app_toast(a, t); }
    }
}

/* ---------- entrada ---------- */
static int s_eq_open;                                     /* painel de presets do EQ */

static int list_count(const App *a) {
    if (a->scan_busy) return 0;
    if (a->tab == TAB_SONGS) return app_songs_rows(a);
    if (a->tab == TAB_FILA) return app_queue_rows(a);
    if (a->tab == TAB_ALBUMS) return a->nalb;
    return a->npl + 2;
}

static void clamp_view(App *a) {
    int n = list_count(a), t = a->tab;
    if (a->sel[t] >= n) a->sel[t] = n - 1;
    if (a->sel[t] < 0) a->sel[t] = 0;
    if (a->sel[t] < a->top[t]) a->top[t] = a->sel[t];
    if (a->sel[t] >= a->top[t] + ROWS) a->top[t] = a->sel[t] - ROWS + 1;
    int max_top = n > ROWS ? n - ROWS : 0;
    if (a->top[t] > max_top) a->top[t] = max_top;
    if (a->top[t] < 0) a->top[t] = 0;
}

static void activate(App *a, int row) {
    if (a->scan_busy) return;
    if (a->tab == TAB_SONGS) { app_play_from(a, app_row_to_id(a, row)); return; }
    if (a->tab == TAB_FILA) { app_queue_play_from(a, row); return; }
    if (a->tab == TAB_ALBUMS) { app_play_album(a, row); return; }
    if (row == 0) app_save_queue_playlist(a);
    else if (row == 1) a->want_rescan = 1;
    else app_open_playlist(a, row - 2);
}

static void tap(App *a, int x, int y) {
    int t = a->tab;
    if (y >= 134 && y < 176 && s_eq_open) {                       /* painel de presets do EQ */
        int p = (x - 8) / 52;
        if (p >= 0 && p < player_eq_count()) player_eq_set(p);
        s_eq_open = 0;
        char b[64]; snprintf(b, sizeof b, "EQ: %s", player_eq_name(player_eq_get()));
        app_toast(a, b);
        return;
    }
    if (y < LIST_Y - 2) {
        if (x < 72) a->tab = TAB_SONGS;
        else if (x < 118) a->tab = TAB_FILA;
        else if (x < 172) a->tab = TAB_ALBUMS;
        else if (x < 240) { a->tab = TAB_PLAYLISTS; app_refresh_playlists(a); }
        else if (x >= 282) screens_power(a, 1);
        else if (t == TAB_SONGS) {
            if (a->query[0]) { app_set_filter(a, ""); a->sel[TAB_SONGS] = a->top[TAB_SONGS] = 0; }  /* limpa */
            else ask_query(a);
        }
    } else if (y < LIST_Y + ROWS * ROW_H) {
        int row = a->top[t] + (y - LIST_Y) / ROW_H;
        if (row < list_count(a)) {
            if (t == TAB_FILA && x >= BOT_W - 34) {               /* lixeira na borda do item */
                app_queue_remove(a, row);
            } else { a->sel[t] = row; activate(a, row); }
        }
    } else if (y >= 176) {
        if (x >= 274) s_eq_open = !s_eq_open;                     /* abre/fecha o painel do EQ */
        else switch (x / 52) {
            case 0: app_toggle_shuffle(a); break;
            case 1: app_prev(a); break;
            case 2: app_toggle_pause(a); break;
            case 3: app_next(a, 1); break;
            default: app_cycle_repeat(a); break;
        }
    }
}

int ui_input(App *a) {
    hidScanInput();
    u32 kd = hidKeysDown(), kr = hidKeysDownRepeat(), ku = hidKeysUp(), kh = hidKeysHeld();
    touchPosition tp; hidTouchRead(&tp);
    int dirty = (kd || ku || (kh & KEY_TOUCH)) ? 1 : 0;

    if (a->screen_off) { if (kd) screens_power(a, 0); return 1; }   /* qualquer toque/botao acorda */

    if (kd & KEY_START) a->quit = 1;
    if (kd & KEY_SELECT) {
        a->tab = (Tab)((a->tab + 1) % TAB_N);
        if (a->tab == TAB_PLAYLISTS) app_refresh_playlists(a);
    }
    int t = a->tab;
    if (kr & (KEY_DUP | KEY_CPAD_UP))     a->sel[t]--;
    if (kr & (KEY_DDOWN | KEY_CPAD_DOWN)) a->sel[t]++;
    if (a->scan_busy && (kd & KEY_B)) a->scan_abort = 1;            /* cancela o scan em thread */
    else if (kd & KEY_B) app_toggle_pause(a);
    if (!a->scan_busy && (kd & KEY_A)) activate(a, a->sel[t]);
    if (t == TAB_FILA && !a->scan_busy) {                           /* gerenciar a fila */
        if (kd & KEY_X) app_queue_remove(a, a->sel[t]);
        if (kr & KEY_DLEFT)  a->sel[t] = app_queue_move(a, a->sel[t], -1);
        if (kr & KEY_DRIGHT) a->sel[t] = app_queue_move(a, a->sel[t], +1);
    } else {
        if (kd & KEY_X) app_toggle_shuffle(a);
        if (kr & KEY_DLEFT)  app_seek_rel(a, -10000);
        if (kr & KEY_DRIGHT) app_seek_rel(a, 10000);
    }
    if (kd & KEY_Y) app_cycle_repeat(a);
    if (kd & KEY_L) app_prev(a);
    if (kd & KEY_R) app_next(a, 1);
    if (s_eq_open && (kd & (KEY_B | KEY_A | KEY_X | KEY_Y))) s_eq_open = 0;
    clamp_view(a);

    if (kd & KEY_TOUCH) { t_down = 1; t_sx = tp.px; t_sy = t_ly = tp.py; t_moved = 0; t_acc = 0; }
    else if ((kh & KEY_TOUCH) && t_down) {
        if (abs((int)tp.py - t_sy) > 6) t_moved = 1;
        if (t_moved && t_sy >= LIST_Y && t_sy < LIST_Y + ROWS * ROW_H) {   /* arrastar rola a lista por linhas */
            t_acc += (int)tp.py - t_ly;
            while (t_acc >= ROW_H)  { a->top[t]--; t_acc -= ROW_H; }
            while (t_acc <= -ROW_H) { a->top[t]++; t_acc += ROW_H; }
            int n = list_count(a), mt = n > ROWS ? n - ROWS : 0;
            if (a->top[t] < 0) a->top[t] = 0;
            if (a->top[t] > mt) a->top[t] = mt;
        }
        t_ly = tp.py;
    }
    if ((ku & KEY_TOUCH) && t_down) { t_down = 0; if (!t_moved) tap(a, t_sx, t_sy); }
    clamp_view(a);
    return dirty;
}

/* ---------- desenho ---------- */
static void draw_top(App *a, const Viz *v) {
    C2D_TargetClear(s_top, BG);
    C2D_SceneBegin(s_top);
    const Track *tr = (a->cur >= 0 && a->cur < a->lib.count) ? &a->lib.tracks[a->cur] : NULL;
    const CoverTex *cv = tr ? covers_get(tr->art_key, tr->path) : NULL;
    u32 accent = ACCENT0;

    if (cv) {   /* fundo "borrado": mini-capa 16x16 esticada com filtro linear + escurecimento */
        C2D_DrawImageAt(cv->mini_img, 0, 0, 0.1f, NULL, (float)TOP_W / COVER_MINI, 240.0f / COVER_MINI);
        C2D_DrawRectSolid(0, 0, 0.2f, TOP_W, 240, C2D_Color32(0, 0, 0, 175));
        accent = cv->accent;
    } else {
        C2D_DrawRectangle(0, 0, 0.1f, TOP_W, 240, C2D_Color32(28, 24, 52, 255), C2D_Color32(28, 24, 52, 255), BG, BG);
    }

    /* capa */
    if (cv) C2D_DrawImageAt(cv->img, 24, 36, 0.4f, NULL, 1.0f, 1.0f);
    else {
        rrect(24, 36, 128, 128, 10, PANEL);
        C2D_DrawCircleSolid(78, 124, 0.6f, 10, ACCENT0); rect(86, 70, 5, 54, ACCENT0); rect(86, 70, 22, 6, ACCENT0);
    }

    char b[160], t2[48];
    if (tr) {
        u_utf8_trunc(b, sizeof b, tr->title, 22);  text(b, 170, 38, 0.62f, TXT);
        u_utf8_trunc(b, sizeof b, tr->artist, 28); text(b, 170, 66, 0.48f, accent);
        u_utf8_trunc(b, sizeof b, tr->album, 30);  text(b, 170, 88, 0.42f, DIM);
    } else {
        text("mp3ds", 170, 50, 0.7f, TXT);
        text("Escolha uma musica na tela de baixo", 170, 82, 0.4f, DIM);
    }

    /* visualizador: 8 barras */
    for (int i = 0; i < VIZ_BANDS; i++) {
        float h = 4.0f + v->level[i] * 56.0f;
        rect(170.0f + i * 26.0f, 174.0f - h, 20, h, accent);
    }

    /* progresso */
    uint32_t pos = player_pos_ms(), dur = player_duration_ms();
    rect(24, 196, 352, 5, C2D_Color32(255, 255, 255, 50));
    if (dur) { float f = (float)pos / dur; if (f > 1) f = 1; rect(24, 196, 352 * f, 5, accent); }
    fmt_time(b, sizeof b, pos); fmt_time(t2, sizeof t2, dur);
    text(b, 24, 206, 0.42f, TXT);
    text_center(t2, 376 - 12, 206, 0.42f, DIM);
    if (a->q.shuffle) text("SHUFFLE", 150, 206, 0.36f, accent);
    if (a->q.repeat != REPEAT_OFF) text(a->q.repeat == REPEAT_ONE ? "REPEAT 1" : "REPEAT", 215, 206, 0.36f, accent);
    if (player_eq_get() > 0) text(player_eq_name(player_eq_get()), 286, 206, 0.34f, ACCENT0);
    if (a->toast_frames > 0) text_center(a->toast, TOP_W / 2.0f, 8, 0.42f, TXT);
}

static void draw_scan(App *a) {
    text_center("Escaneando a biblioteca...", BOT_W / 2.0f, 90, 0.55f, TXT);
    char b[64];
    snprintf(b, sizeof b, "%d faixa(s) encontrada(s)", a->scan_found);
    text_center(b, BOT_W / 2.0f, 120, 0.45f, DIM);
    text_center("B cancela o scan", BOT_W / 2.0f, 150, 0.4f, DIM);
}

static void draw_bottom(App *a) {
    C2D_TargetClear(s_bot, BG);
    C2D_SceneBegin(s_bot);
    int t = a->tab;
    const Track *cur = (a->cur >= 0 && a->cur < a->lib.count) ? &a->lib.tracks[a->cur] : NULL;
    const CoverTex *cv = cur ? covers_get(cur->art_key, cur->path) : NULL;
    u32 accent = cv ? cv->accent : ACCENT0;

    /* cabecalho: Musicas | Fila | Albuns | Playlists | lupa | lua */
    rect(0, 0, BOT_W, 28, PANEL);
    text("Musicas", 8, 5, 0.44f, t == TAB_SONGS ? TXT : DIM);
    text("Fila", 80, 5, 0.44f, t == TAB_FILA ? TXT : DIM);
    text("Albuns", 124, 5, 0.44f, t == TAB_ALBUMS ? TXT : DIM);
    text("Playlists", 176, 5, 0.44f, t == TAB_PLAYLISTS ? TXT : DIM);
    static const float ux[4] = {4, 76, 120, 172}, uw[4] = {62, 40, 50, 66};
    rect(ux[t], 25, uw[t], 3, accent);
    if (t == TAB_SONGS && a->query[0]) {
        icon_clear(263, 14, accent);
        if (a->nfilt == 0) text("(vazio)", 192, 16, 0.3f, DIM);
    } else {
        icon_search(263, 14, t == TAB_SONGS ? TXT : DIM);
    }
    C2D_DrawCircleSolid(296, 14, 0.6f, 7, DIM); C2D_DrawCircleSolid(299, 12, 0.7f, 6, PANEL);   /* "lua": tela off */

    if (a->scan_busy) { draw_scan(a); return; }

    /* lista */
    int n = list_count(a);
    char b[160], c[96];
    if (t == TAB_SONGS && n == 0) {
        text_center(a->query[0] ? "Nada encontrado" : "Biblioteca vazia", BOT_W / 2.0f, 80, 0.5f, DIM);
        if (!a->query[0]) text_center("Coloque MP3 em sdmc:/Music", BOT_W / 2.0f, 106, 0.4f, DIM);
    } else if (t == TAB_FILA && n == 0) {
        text_center("Fila vazia", BOT_W / 2.0f, 70, 0.5f, DIM);
        text_center("Toque uma faixa na aba Musicas", BOT_W / 2.0f, 96, 0.4f, DIM);
    } else if (t == TAB_ALBUMS && n == 0) {
        text_center("Nenhum album", BOT_W / 2.0f, 80, 0.5f, DIM);
    } else if (t == TAB_FILA && n > 0) {
        snprintf(b, sizeof b, "X ou lixo: remover   < / > : mover");
        text_center(b, BOT_W / 2.0f, LIST_Y - 4, 0.3f, DIM);
    }
    for (int r = 0; r < ROWS && a->top[t] + r < n; r++) {
        int i = a->top[t] + r; float y = LIST_Y + r * ROW_H;
        if (i == a->sel[t]) rect(0, y, BOT_W - 8, ROW_H - 2, PANEL);
        if (t == TAB_SONGS) {
            const Track *tr = &a->lib.tracks[a->filt ? a->filt[i] : i];
            u_utf8_trunc(b, sizeof b, tr->title, 34);
            u_utf8_trunc(c, sizeof c, tr->artist, 36);
            int playing = (!a->query[0] && i == a->cur) || (a->query[0] && a->nfilt > 0 && a->filt[i] == a->cur);
            if (playing) rect(0, y, 3, ROW_H - 2, accent);
            text(b, 10, y + 1, 0.45f, playing ? accent : TXT);
            text(c, 10, y + 15, 0.34f, DIM);
        } else if (t == TAB_FILA) {
            const Track *tr = &a->lib.tracks[app_queue_slot_id(a, i)];
            u_utf8_trunc(b, sizeof b, tr->title, 30);
            u_utf8_trunc(c, sizeof c, tr->artist, 30);
            int playing = (i == queue_current_slot(&a->q));
            if (playing) rect(0, y, 3, ROW_H - 2, accent);
            text(b, 10, y + 1, 0.45f, playing ? accent : TXT);
            text(c, 10, y + 15, 0.34f, DIM);
            icon_clear(304, y + 13, i == a->sel[t] ? DIM : C2D_Color32(90, 90, 105, 255));   /* lixeira */
        } else if (t == TAB_ALBUMS) {
            const AlbumGroup *al = &a->albums[i];
            int playing = a->cur >= 0 && a->cur >= al->first && a->cur < al->first + al->count;
            if (al->album[0]) { u_utf8_trunc(b, sizeof b, al->album, 34); u_utf8_trunc(c, sizeof c, al->artist, 36); }
            else { u_utf8_trunc(b, sizeof b, al->artist, 34); snprintf(c, sizeof c, "%d faixa(s)", al->count); }
            if (playing) rect(0, y, 3, ROW_H - 2, accent);
            text(b, 10, y + 1, 0.45f, playing ? accent : TXT);
            text(c, 10, y + 15, 0.34f, DIM);
        } else {
            if (i == 0) text("+  Salvar fila atual como playlist", 10, y + 5, 0.45f, TXT);
            else if (i == 1) text("Reescanear biblioteca", 10, y + 5, 0.45f, TXT);
            else { u_utf8_trunc(b, sizeof b, a->pls[i - 2].name, 36); text(b, 10, y + 5, 0.45f, TXT); }
        }
    }
    if (n > ROWS) {   /* barra de rolagem */
        float h = (float)ROWS / n * (ROWS * ROW_H); if (h < 12) h = 12;
        float y = LIST_Y + (float)a->top[t] / (n - ROWS) * (ROWS * ROW_H - h);
        rect(BOT_W - 4, y, 3, h, DIM);
    }

    /* painel visual do EQ (aberto pelo botao EQ) */
    int eq = player_eq_get();
    if (s_eq_open) {
        rrect(4, 132, BOT_W - 8, 42, 8, PANEL);
        for (int p = 0; p < player_eq_count(); p++) {
            float x = 8 + p * 52;
            rrect(x, 136, 48, 34, 5, p == eq ? accent : PANEL);
            for (int k = 0; k < 5; k++) {                       /* perfil de frequencia do preset */
                float bh = EQ_BARS[p][k] * 0.8f;
                rect(x + 10 + k * 6, 148 - bh, 4, bh, p == eq ? BG : DIM);
            }
            text_center(player_eq_name(p), x + 24, 152, 0.26f, p == eq ? BG : TXT);
        }
    }

    /* controles: shuffle | prev | play | next | repeat | EQ */
    rect(0, 176, BOT_W, 64, PANEL);
    float cy = 208;
    u32 sh = a->q.shuffle ? accent : DIM, rp = a->q.repeat != REPEAT_OFF ? accent : DIM;
    tri(38, cy - 7, 38, cy + 7, 52, cy, sh); tri(38, cy + 1, 38, cy + 13, 50, cy + 7, sh);    /* shuffle (simbolico) */
    icon_prev(96, cy, 22, TXT);
    C2D_DrawCircleSolid(156, cy, 0.5f, 24, accent);
    if (player_state() == PS_PLAYING) icon_pause(156, cy, 22, BG); else icon_play(156, cy, 22, BG);
    icon_next(216, cy, 22, TXT);
    rrect(240, cy - 9, 28, 18, 5, rp); rect(245, cy - 5, 18, 10, PANEL);                     /* repeat (simbolico) */
    if (a->q.repeat == REPEAT_ONE) rect(252, cy - 4, 3, 8, rp);
    C2D_DrawCircleSolid(296, cy, 0.5f, 14, eq ? accent : PANEL);
    text_center("EQ", 296, cy - 6, 0.35f, eq ? BG : DIM);
}

void ui_draw(App *a) {
    static Viz viz;
    player_get_viz(&viz);
    if (a->screen_off) return;
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TextBufClear(s_buf);
    draw_top(a, &viz);
    draw_bottom(a);
    C3D_FrameEnd(0);
}

void ui_message(const char *l1, const char *l2) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TextBufClear(s_buf);
    C2D_TargetClear(s_top, BG); C2D_SceneBegin(s_top);
    text_center(l1, TOP_W / 2.0f, 100, 0.6f, TXT);
    C2D_TargetClear(s_bot, BG); C2D_SceneBegin(s_bot);
    text_center(l2, BOT_W / 2.0f, 100, 0.45f, DIM);
    C3D_FrameEnd(0);
}
