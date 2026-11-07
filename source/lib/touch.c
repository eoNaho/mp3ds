#include "touch.h"

void touch_begin(TouchGesture *g, int x, int y) {
    g->active = 1; g->moved = 0;
    g->x = g->sx = x; g->y = g->sy = y;
    g->acc = 0;
}

int touch_drag(TouchGesture *g, int x, int y, int row_h) {
    if (!g->active || row_h <= 0) return 0;
    int dy = y - g->y;
    g->x = x; g->y = y;
    if (!g->moved) {
        int ox = x - g->sx, oy = y - g->sy;
        if (ox * ox + oy * oy > TOUCH_SLOP_PX * TOUCH_SLOP_PX) g->moved = 1;
        return 0; /* comeca a acumular daqui, sem salto ao passar do limite */
    }
    g->acc += dy;
    int rows = 0;
    while (g->acc <= -row_h) { rows++; g->acc += row_h; }
    while (g->acc >= row_h) { rows--; g->acc -= row_h; }
    return rows;
}

int touch_end(TouchGesture *g) {
    int tap = g->active && !g->moved;
    g->active = 0;
    return tap;
}

int touch_control_at(int x) {
    if (x < 70) return CTL_SHUFFLE;
    if (x < 126) return CTL_PREV;
    if (x < 186) return CTL_PLAY;
    if (x < 235) return CTL_NEXT;
    if (x < 275) return CTL_REPEAT;
    return CTL_EQ;
}
