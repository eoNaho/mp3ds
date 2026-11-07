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
#include "tap_confirm.h"

#define ROW_H    28
#define ROWS     5
#define LIST_Y   30
#define TOP_W    400
#define BOT_W    320
#define MAX_QUERY 96

static C3D_RenderTarget *s_top, *s_bot;
static C2D_TextBuf s_buf;
static u32 BG, PANEL, TXT, DIM, ACCENT0;

/* estado do toque */
static int s_actions_id = -1, s_a_consumed, s_search_open;
static const char *SEARCH_FIELDS[] = {"Todos", "Titulo", "Artista", "Album", "Pasta"};
static const char *SEARCH_PLAYS[] = {"Todas", "Nunca tocadas", "Ja tocadas"};
static int t_down, t_sx, t_sy, t_ly, t_moved, t_acc;
static TapConfirm s_tap;

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
    swkbdSetValidation(&sw, SWKBD_ANYTHING, 0, 0);
    swkbdSetHintText(&sw, "Combine palavras: artista e titulo");
    swkbdSetInitialText(&sw, a->query);                   /* App persiste durante a chamada */
    char buf[MAX_QUERY + 1];
    if (swkbdInputText(&sw, buf, sizeof buf) == SWKBD_BUTTON_CONFIRM) {
        app_set_filter(a, buf);
        int n = app_songs_rows(a);
        if (n == 0) app_toast(a, "Nada encontrado");
        else { char t[64]; snprintf(t, sizeof t, "%d faixa(s) encontrada(s)", n); app_toast(a, t); }
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

static void cycle_tab(App *a, int dir) {
    int next = (int)a->tab + dir;
    if (next < 0) next = TAB_N - 1;
    if (next >= TAB_N) next = 0;
    a->tab = (Tab)next;
    if (a->tab == TAB_PLAYLISTS) app_refresh_playlists(a);
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

static void search_action(App *a, int action) {
    if (action == 0) { ask_query(a); return; }
    if (action == 1) a->search_field = (SearchField)((a->search_field + 1) % SEARCH_FIELD_N);
    if (action == 2) a->search_plays = (SearchPlays)((a->search_plays + 1) % SEARCH_PLAY_N);
    if (action == 3) {
        a->search_field = SEARCH_ALL; a->search_plays = SEARCH_ANY_PLAY;
        app_set_filter(a, ""); return;
    }
    if (action == 4) { s_search_open = 0; return; }
    app_set_filter(a, a->query);
}

static void tap(App *a, int x, int y) {
    int t = a->tab;
    /* So a mesma linha/controle pode completar o toque duplo. */
    int row = a->top[t] + (y - LIST_Y) / ROW_H;
    int list_play = y >= LIST_Y && y < LIST_Y + ROWS * ROW_H &&
                    row < list_count(a) &&
                    !((t == TAB_SONGS || t == TAB_FILA) && x >= BOT_W - 34);
    int skip = y >= 176 && x >= 52 && x < 208 && x / 52 != 2;
    int confirm = 0;
    if (!s_search_open && s_actions_id < 0 &&
        !(s_eq_open && y >= 134 && y < 176) && (list_play || skip))
        confirm = tap_confirm(&s_tap, list_play ? t : TAB_N,
                              list_play ? row : x / 52, osGetTime());
    else tap_confirm_reset(&s_tap);
    if (s_search_open) {
        if (y >= 34 && y < 74) search_action(a, 0);
        else if (y >= 78 && y < 112) search_action(a, 1);
        else if (y >= 116 && y < 150) search_action(a, 2);
        else if (y >= 190) search_action(a, x < 160 ? 3 : 4);
        return;
    }
    if (s_actions_id >= 0) {
        if (x >= 20 && x < 300 && y >= 48 && y < 144) {
            int id = s_actions_id;
            s_actions_id = -1;
            app_enqueue(a, id, y < 96);
        } else s_actions_id = -1;
        return;
    }
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
        else if (x < 210) { a->tab = TAB_PLAYLISTS; app_refresh_playlists(a); }
        else if (x >= 282) screens_power(a, 1);
        else if (x >= 210 && x < 245 && t == TAB_SONGS) app_cycle_sort(a);
        else if (x >= 245 && x < 282 && t == TAB_SONGS && !a->scan_busy) {
            s_search_open = 1; s_eq_open = 0;
        }
    } else if (y < LIST_Y + ROWS * ROW_H) {
        int row = a->top[t] + (y - LIST_Y) / ROW_H;
        if (row < list_count(a)) {
            if (t == TAB_SONGS && x >= BOT_W - 34) {
                a->sel[t] = row;
                s_actions_id = app_row_to_id(a, row);
                s_eq_open = 0;
            } else if (t == TAB_FILA && x >= BOT_W - 34) {               /* lixeira na borda do item */
                app_queue_remove(a, row);
            } else {
                a->sel[t] = row;
                if (confirm) activate(a, row);
                else app_toast(a, "Toque duas vezes para abrir/tocar");
            }
        }
    } else if (y >= 176) {
        if (x >= 274) s_eq_open = !s_eq_open;                     /* abre/fecha o painel do EQ */
        else switch (x / 52) {
            case 0: app_toggle_shuffle(a); break;
            case 1: if (confirm) app_prev(a); else app_toast(a, "Toque duas vezes para voltar"); break;
            case 2: app_toggle_pause(a); break;
            case 3: if (confirm) app_next(a, 1); else app_toast(a, "Toque duas vezes para avancar"); break;
            default: app_cycle_repeat(a); break;
        }
    }
}

int ui_input(App *a) {
    hidScanInput();
    u32 kd = hidKeysDown(), kr = hidKeysDownRepeat(), ku = hidKeysUp(), kh = hidKeysHeld();
    touchPosition tp; hidTouchRead(&tp);
    int dirty = (kd || ku || (kh & KEY_TOUCH)) ? 1 : 0;

    if (kd & KEY_START) { a->quit = 1; return 1; }
    if (kd & ~KEY_TOUCH) tap_confirm_reset(&s_tap);
    if (a->screen_off) { tap_confirm_reset(&s_tap); if (kd) screens_power(a, 0); return 1; }   /* qualquer toque/botao acorda */

    if (s_search_open && a->scan_busy) s_search_open = 0;
    if (s_search_open) {
        s_a_consumed = 1;
        if (kd & KEY_B) search_action(a, 4);
        else if (kd & KEY_A) search_action(a, 0);
        else if (kd & KEY_X) search_action(a, 1);
        else if (kd & KEY_Y) search_action(a, 2);
        if (kd & KEY_TOUCH) { t_down = 1; t_sx = tp.px; t_sy = tp.py; }
        if ((ku & KEY_TOUCH) && t_down) { t_down = 0; tap(a, t_sx, t_sy); }
        return dirty;
    }
    if (kd & KEY_SELECT) {
        a->tab = (Tab)((a->tab + 1) % TAB_N);
        if (a->tab == TAB_PLAYLISTS) app_refresh_playlists(a);
    }

    if (kd & KEY_A) s_a_consumed = 0;
    if (s_actions_id >= 0 && a->scan_busy) s_actions_id = -1;
    if (s_actions_id >= 0 && (kd & (KEY_A | KEY_B | KEY_X | KEY_Y))) {
        int id = s_actions_id; s_actions_id = -1; s_a_consumed = 1;
        if (kd & (KEY_A | KEY_X)) app_enqueue(a, id, 1);
        else if (kd & KEY_Y) app_enqueue(a, id, 0);
        return dirty;
    }
    /* A + L/R troca de aba; L/R sozinho continua sendo anterior/proxima faixa.
       Aceita tanto A pressionado antes de L/R quanto os dois juntos. */
    int tab_left = ((kd & KEY_L) && (kh & KEY_A)) || ((kd & KEY_A) && (kh & KEY_L));
    int tab_right = ((kd & KEY_R) && (kh & KEY_A)) || ((kd & KEY_A) && (kh & KEY_R));
    if (tab_left || tab_right) {
        s_a_consumed = 1;
        cycle_tab(a, tab_left ? -1 : 1);
        clamp_view(a);
        return dirty;
    }

    int t = a->tab;
    if (t == TAB_SONGS && !a->scan_busy && (kh & KEY_A) &&
        ((kd & (KEY_X | KEY_Y)) || ((kd & KEY_A) && (kh & (KEY_X | KEY_Y))))) {
        app_enqueue(a, app_row_to_id(a, a->sel[t]), !!(kh & KEY_X));
        s_a_consumed = 1;
        return dirty;
    }
    if (kr & (KEY_DUP | KEY_CPAD_UP))     a->sel[t]--;
    if (kr & (KEY_DDOWN | KEY_CPAD_DOWN)) a->sel[t]++;
    if (a->scan_busy && (kd & KEY_B)) a->scan_abort = 1;            /* cancela o scan em thread */
    else if (kd & KEY_B) app_toggle_pause(a);
    if (!a->scan_busy && t == TAB_SONGS && (ku & KEY_A) && !s_a_consumed)
        activate(a, a->sel[t]);
    if (!a->scan_busy && t != TAB_SONGS && (kd & KEY_A)) activate(a, a->sel[t]);
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
        if (abs((int)tp.py - t_sy) > 6 || abs((int)tp.px - t_sx) > 6) {
            t_moved = 1;
            tap_confirm_reset(&s_tap);
        }
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
    const CoverTex *cv = tr ? covers_get(&a->lib, a->cur) : NULL;
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
    const CoverTex *cv = cur ? covers_get(&a->lib, a->cur) : NULL;
    u32 accent = cv ? cv->accent : ACCENT0;

    if (s_search_open) {
        char b[160], query[160];
        text_center("Busca e filtros", 160, 8, 0.6f, TXT);
        rrect(8, 34, 304, 40, 6, PANEL);
        u_utf8_trunc(query, sizeof query, a->query[0] ? a->query : "Toque para digitar", 36);
        text(query, 16, 43, 0.42f, TXT);
        rrect(8, 78, 304, 34, 6, PANEL);
        snprintf(b, sizeof b, "X  Buscar em: %s", SEARCH_FIELDS[a->search_field]);
        text(b, 16, 86, 0.45f, TXT);
        rrect(8, 116, 304, 34, 6, PANEL);
        snprintf(b, sizeof b, "Y  Reproducoes: %s", SEARCH_PLAYS[a->search_plays]);
        text(b, 16, 124, 0.45f, TXT);
        snprintf(b, sizeof b, "%d de %d faixas", app_songs_rows(a), a->lib.count);
        text_center(b, 160, 160, 0.44f, accent);
        rrect(8, 190, 146, 40, 6, PANEL);
        rrect(166, 190, 146, 40, 6, accent);
        text_center("Limpar tudo", 81, 202, 0.45f, TXT);
        text_center("B  Ver resultados", 239, 202, 0.4f, BG);
        return;
    }
    /* cabecalho: Musicas | Fila | Albuns | Playlists | lupa | lua */
    rect(0, 0, BOT_W, 28, PANEL);
    text("Musicas", 8, 5, 0.44f, t == TAB_SONGS ? TXT : DIM);
    text("Fila", 80, 5, 0.44f, t == TAB_FILA ? TXT : DIM);
    text("Albuns", 124, 5, 0.44f, t == TAB_ALBUMS ? TXT : DIM);
    text("Playlists", 176, 5, 0.44f, t == TAB_PLAYLISTS ? TXT : DIM);
    static const float ux[4] = {4, 76, 120, 172}, uw[4] = {62, 40, 50, 66};
    rect(ux[t], 25, uw[t], 3, accent);
    icon_search(263, 14, t == TAB_SONGS && app_filter_active(a) ? accent : DIM);
    if (t == TAB_SONGS) text(app_sort_name(a->sort_mode), 218, 5, 0.28f, DIM);
    C2D_DrawCircleSolid(296, 14, 0.6f, 7, DIM); C2D_DrawCircleSolid(299, 12, 0.7f, 6, PANEL);   /* "lua": tela off */

    if (a->scan_busy) { draw_scan(a); return; }

    /* lista */
    int n = list_count(a);
    char b[160], c[96];
    if (t == TAB_SONGS && n == 0) {
        text_center(app_filter_active(a) ? "Nada encontrado" : "Biblioteca vazia", BOT_W / 2.0f, 80, 0.5f, DIM);
        if (!app_filter_active(a)) text_center("Coloque MP3/FLAC em sdmc:/Music", BOT_W / 2.0f, 106, 0.4f, DIM);
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
            const Track *tr = &a->lib.tracks[app_row_to_id(a, i)];
            u_utf8_trunc(b, sizeof b, tr->title, 30);
            char artist[80]; u_utf8_trunc(artist, sizeof artist, tr->artist, 22);
            snprintf(c, sizeof c, "%s  [%u]", artist, (unsigned)tr->plays);
            int playing = (app_row_to_id(a, i) == a->cur);
            if (playing) rect(0, y, 3, ROW_H - 2, accent);
            text(b, 10, y + 1, 0.45f, playing ? accent : TXT);
            text(c, 10, y + 15, 0.34f, DIM);
            text("+", 295, y + 2, 0.65f, accent);
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

    if (s_actions_id >= 0) {
        rrect(16, 42, 288, 116, 8, PANEL);
        text_center("Adicionar a fila", 160, 46, 0.4f, DIM);
        text("A/X: tocar a seguir", 28, 68, 0.48f, TXT);
        text("Y: adicionar ao fim", 28, 112, 0.48f, TXT);
        text_center("B ou toque fora: cancelar", 160, 144, 0.3f, DIM);
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
    if (a->screen_off) { gspWaitForVBlank(); return; }
    static Viz viz;
    static unsigned viz_frame;
    /* O visualizador e apenas decorativo: 15 Hz reduz calculos de float sem
       deixar a interface perceptivelmente menos fluida. */
    if ((viz_frame++ & 1u) == 0 || player_state() != PS_PLAYING) player_get_viz(&viz);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    covers_pump(); /* troca texturas somente depois de concluir o frame anterior */
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
