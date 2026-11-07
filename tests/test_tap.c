#include <assert.h>
#include <stdio.h>
#include "tap_confirm.h"

int main(void) {
    TapConfirm t = {0};
    assert(!tap_confirm(&t, 0, 2, 1000));
    assert(tap_confirm(&t, 0, 2, 1450));
    /* Um terceiro toque nao troca de faixa outra vez. */
    assert(!tap_confirm(&t, 0, 2, 1500));
    assert(!tap_confirm(&t, 0, 2, 1951));
    assert(!tap_confirm(&t, 0, 3, 2000));
    assert(!tap_confirm(&t, 1, 3, 2050));
    assert(tap_confirm(&t, 1, 3, 2100));
    assert(!tap_confirm(&t, 0, 2, 2200));
    tap_confirm_reset(&t); /* arrastar, mudar de aba ou usar outro controle */
    assert(!tap_confirm(&t, 0, 2, 2300));
    assert(tap_confirm(&t, 0, 2, 2400));
    puts("ok tap: toque duplo, prazo, alvo, aba, cancelamento e terceiro toque");
    return 0;
}
