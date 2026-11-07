#pragma once
/* Gestos e zonas de toque portaveis, sem libctru. */
#define TOUCH_SLOP_PX 10

typedef struct {
    int active, moved;
    int x, y;       /* ultima posicao valida */
    int sx, sy;     /* posicao inicial */
    int acc;        /* resto do arrasto vertical em pixels */
} TouchGesture;

void touch_begin(TouchGesture *g, int x, int y);
/* +1 = mostrar linhas abaixo (dedo subindo), -1 = linhas acima. */
int touch_drag(TouchGesture *g, int x, int y, int row_h);
/* 1 = toque; usar g->x/g->y. 0 = arrasto ou nenhum gesto ativo. */
int touch_end(TouchGesture *g);

/* Fronteiras entre os centros dos icones: 45, 96, 156, 216, 254, 296. */
enum { CTL_SHUFFLE = 0, CTL_PREV, CTL_PLAY, CTL_NEXT, CTL_REPEAT, CTL_EQ, CTL_N };
int touch_control_at(int x);
