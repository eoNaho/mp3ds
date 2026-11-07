#pragma once
#include <stdint.h>
/* RGBA linear -> ABGR tiled 8x8, preservando a orientacao da imagem, para PICA200.
   Citro2D usa subtextura top=1/bottom=0; nao inverter as linhas aqui.
   w e h devem ser multiplos de 8; dst precisa de w*h*4 bytes. */
void texture_tile_rgba8(uint8_t *dst, const uint8_t *rgba, int w, int h);
