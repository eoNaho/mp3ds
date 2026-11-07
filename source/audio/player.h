#pragma once
#include <stdint.h>
#include "viz.h"

typedef enum { PS_STOPPED = 0, PS_PLAYING, PS_PAUSED } PlayerState;

int  player_init(void);              /* 0 = ok; != 0 normalmente = falta o dspfirm.cdc */
void player_exit(void);
void player_play(const char *path);  /* assincrono: a thread de audio abre o arquivo */
void player_play_seek(const char *path, uint32_t start_ms);  /* idem, ja comecando em start_ms (retomada) */
void player_stop(void);
void player_pause(int paused);
void player_seek(uint32_t ms);
PlayerState player_state(void);
uint32_t player_pos_ms(void);
uint32_t player_duration_ms(void);
/* 1 = faixa terminou, -1 = erro ao abrir/decodificar, 0 = nada novo. Consome o evento. */
int  player_take_finished(void);
void player_get_viz(Viz *out);       /* analisa o trecho que esta tocando agora; chamar 1x por frame */
/* equalizador (pico IIR do DSP, no canal do player) */
void player_eq_set(int preset);
void player_eq_cycle(void);
int  player_eq_get(void);
int  player_eq_count(void);
const char *player_eq_name(int preset);
