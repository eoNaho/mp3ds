#pragma once
#include "app.h"

void ui_init(void);
void ui_exit(void);
void ui_message(const char *line1, const char *line2);  /* tela cheia simples (scan, erros) */
int  ui_input(App *a);                                   /* le botoes/toque; retorna 1 se algo mudou */
void ui_draw(App *a);                                    /* desenha as duas telas */
