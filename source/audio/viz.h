#pragma once
#include <stdint.h>

#define VIZ_BANDS 8
typedef struct { float level[VIZ_BANDS]; } Viz;   /* 0..1 */

void viz_init(Viz *v);
/* Analisa ate 512 frames estereo (Goertzel em 8 frequencias) e atualiza os niveis (ataque rapido, queda suave). */
void viz_feed(Viz *v, const int16_t *stereo, int frames, int hz);
void viz_decay(Viz *v);   /* chamar quando nao ha audio novo */
