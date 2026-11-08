#pragma once
#include "app.h"

void ui_init(void);
void ui_exit(void);
void ui_message(const char *line1, const char *line2);  /* tela cheia simples (scan, erros) */
int  ui_input(App *a);                                   /* consome HID ja lido pelo main; retorna 1 se algo mudou */
void ui_draw(App *a);                                    /* desenha as duas telas */

void ui_exit_message(void); /* tenta desenhar sem bloquear a saida */

void ui_record_loop(unsigned loop_ms, unsigned tick_ms); /* picos DIAG2 em ms */
