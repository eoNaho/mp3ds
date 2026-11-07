#pragma once
#include <stdint.h>

typedef struct {
    int pending, scope, target;
    uint64_t ms;
} TapConfirm;

static inline void tap_confirm_reset(TapConfirm *t) { t->pending = 0; }

/* Dois toques consecutivos no mesmo alvo, em ate 450 ms. */
static inline int tap_confirm(TapConfirm *t, int scope, int target, uint64_t ms) {
    if (t->pending && t->scope == scope && t->target == target &&
        ms >= t->ms && ms - t->ms <= 450) {
        tap_confirm_reset(t);
        return 1;
    }
    *t = (TapConfirm){1, scope, target, ms};
    return 0;
}
