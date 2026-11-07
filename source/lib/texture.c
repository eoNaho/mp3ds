#include "texture.h"
#include <stddef.h>

void texture_tile_rgba8(uint8_t *dst, const uint8_t *rgba, int w, int h) {
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        unsigned tx = (unsigned)x & 7, ty = (unsigned)y & 7;
        unsigned morton = (tx & 1) | ((ty & 1) << 1) |
            ((tx & 2) << 1) | ((ty & 2) << 2) |
            ((tx & 4) << 2) | ((ty & 4) << 3);
        size_t pixel = ((size_t)(y / 8) * (w / 8) + x / 8) * 64 + morton;
        const uint8_t *src = rgba + ((size_t)y * w + x) * 4;
        uint8_t *out = dst + pixel * 4;
        out[0] = src[3]; out[1] = src[2]; out[2] = src[1]; out[3] = src[0];
    }
}
