#include "viz.h"
#include <math.h>
#include <string.h>

static const float CENTER[VIZ_BANDS] = {60, 150, 400, 1000, 2400, 5000, 9000, 14000};
static float s_hann[512];
static int   s_hann_ready;

void viz_init(Viz *v) { memset(v, 0, sizeof *v); }

void viz_decay(Viz *v) { for (int i = 0; i < VIZ_BANDS; i++) v->level[i] *= 0.88f; }

void viz_feed(Viz *v, const int16_t *s, int frames, int hz) {
    if (frames > 512) frames = 512;
    if (frames < 64 || hz <= 0) { viz_decay(v); return; }
    if (!s_hann_ready) {   /* janela de Hann pre-calculada: cosf por amostra seria caro no ARM11 */
        for (int i = 0; i < 512; i++) s_hann[i] = 0.5f - 0.5f * cosf(2.0f * 3.14159265f * (float)i / 511.0f);
        s_hann_ready = 1;
    }
    float mono[512];
    for (int i = 0; i < frames; i++) mono[i] = ((float)s[2 * i] + (float)s[2 * i + 1]) * 0.5f;

    for (int b = 0; b < VIZ_BANDS; b++) {
        float f = CENTER[b], lvl = 0.0f;
        if (f < 0.45f * (float)hz) {
            float coeff = 2.0f * cosf(2.0f * 3.14159265f * f / (float)hz), s1 = 0, s2 = 0;
            for (int i = 0; i < frames; i++) {
                float s0 = mono[i] * s_hann[i] + coeff * s1 - s2;
                s2 = s1; s1 = s0;
            }
            float power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
            float mag = sqrtf(power > 0 ? power : 0) * 4.0f / (float)frames;   /* ~amplitude de pico */
            float db = 20.0f * log10f(mag / 32768.0f + 1e-6f);
            lvl = (db + 54.0f) / 54.0f;
            lvl = lvl < 0 ? 0 : (lvl > 1 ? 1 : lvl);
        }
        float old = v->level[b] * 0.85f;
        v->level[b] = lvl > old ? lvl : old;
    }
}
