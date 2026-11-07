#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "player.h"
#include "config.h"
#include "util.h"
#include "ui.h"
#include "covers.h"

/* pilha da thread principal (padrao ~32 KB e curta para scan recursivo + stdio) */
u32 __stacksize__ = 128 * 1024;

static int on_scan(int found, const char *cur, void *ud) {
    (void)ud; (void)cur;
    if (found % 8 == 0) {
        char b[64];
        snprintf(b, sizeof b, "%d faixas encontradas  (B cancela)", found);
        ui_message("Escaneando a biblioteca...", b);
    }
    hidScanInput();
    return (hidKeysDown() & KEY_B) ? 1 : 0;
}

static void wait_start(void) {
    while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gspWaitForVBlank(); }
}

int main(void) {
    u_mkdir_p(DATA_DIR); u_mkdir_p(COVER_DIR); u_mkdir_p(PLAYLIST_DIR); u_mkdir_p(MUSIC_ROOT);
    ui_init();

    if (player_init() != 0) {
        ui_message("Nao consegui iniciar o audio (DSP)",
                   "Falta o dspfirm.cdc: rode o app DSP1 e tente de novo.  START sai");
        wait_start();
        ui_exit();
        return 1;
    }
    covers_init();

    static App app;
    app_init(&app);
    if (app_needs_initial_scan(&app))
        app_rescan(&app, on_scan, NULL);   /* somente no primeiro uso ou indice ausente/invalido */

    int redraw = 2;   /* 2 frames: as telas do 3DS tem buffer duplo */
    while (aptMainLoop() && !app.quit) {
        if (ui_input(&app)) redraw = 2;
        if (app.quit) break;            /* START nao inicia mais trabalho nem outro frame */
        app_tick(&app);
        /* Enquanto toca, fechar a tampa nao deve suspender o decoder nem o DSP. */
        aptSetSleepAllowed(player_state() != PS_PLAYING);
        if (app.want_rescan) { app.want_rescan = 0; app_start_rescan(&app); redraw = 2; }   /* reescaneia em thread */
        if (covers_ready()) redraw = 2;

        if (redraw > 0 || player_state() == PS_PLAYING || app.toast_frames > 0 || app.scan_busy) {
            ui_draw(&app);
            if (redraw > 0) redraw--;
        } else {
            gspWaitForVBlank();        /* ocioso: nao redesenha, poupa bateria */
        }
    }

    aptSetSleepAllowed(true);
    ui_message("Saindo...", "Salvando a fila e encerrando o audio");
    app_exit(&app);
    player_exit();
    covers_exit();
    ui_exit();
    return 0;
}
