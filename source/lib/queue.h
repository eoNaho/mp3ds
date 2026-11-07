#pragma once

typedef enum { REPEAT_OFF = 0, REPEAT_ALL, REPEAT_ONE } RepeatMode;

typedef struct {
    int *items, *order;     /* items = ids de faixas; order = permutacao (shuffle) */
    int count, pos;         /* pos = posicao dentro de order */
    int shuffle;
    RepeatMode repeat;
    unsigned rng;
} Queue;

void queue_init(Queue *q, unsigned seed);
void queue_free(Queue *q);
int  queue_set(Queue *q, const int *items, int count, int start);  /* start = indice em items */
int  queue_current(const Queue *q);                                /* id ou -1 */
int  queue_next(Queue *q, int manual);                             /* id ou -1 se acabou */
int  queue_prev(Queue *q);
void queue_set_shuffle(Queue *q, int on);                          /* mantem a faixa atual */
void queue_set_pos(Queue *q, int pos);                             /* comeca a contar a fila em pos */
/* gerenciamento da fila por "slot" (posicao de reproducao, ja com shuffle aplicado) */
int  queue_slot_id(const Queue *q, int slot);                      /* id no slot ou -1 */
int  queue_current_slot(const Queue *q);                           /* slot em reproducao */
int  queue_remove_slot(Queue *q, int slot);                        /* remove e devolve o slot atual */
int  queue_move_slot(Queue *q, int slot, int dir);                 /* move ±1; devolve o novo slot */
/* Insere na ordem de reproducao sem mudar a faixa atual; -1 se falhar. */
int  queue_insert(Queue *q, int id, int next);
