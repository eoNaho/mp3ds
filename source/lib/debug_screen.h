#pragma once
#include <stddef.h>
#include <stdint.h>
/* Painel de diagnostico em framebuffer BGR8 rotacionado do 3DS. Sem GPU/stdio/SD. */
void debug_screen_draw(uint8_t *pixels, size_t bytes, int width, int height,
                       unsigned step, const char *stage);
