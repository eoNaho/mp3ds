#pragma once
/* Rastro visual de encerramento. Nao depende de um novo frame da GPU e agora
   grava cada etapa em DATA_DIR/exit.log (SD) no 3DS, sobrevivendo a congelos. */
typedef void (*ExitTraceSink)(unsigned step, const char *stage);
void exit_trace_set_sink(ExitTraceSink sink); /* usado pelos testes host */
void exit_trace_open(void);   /* inicia log do boot; chamar antes das etapas de arranque */
void exit_trace_begin(void);
void exit_trace(const char *stage);
