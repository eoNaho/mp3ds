#include "debug_screen.h"

/* Colunas 5x7, bit 0 no topo. Somente ASCII usado pelo diagnostico. */
static const uint8_t letters[26][5] = {
    {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
    {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
    {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
    {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
    {62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},
    {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
    {7,8,112,8,7},{97,81,73,69,67}
};
static const uint8_t digits[10][5] = {
    {62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},
    {24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},
    {54,73,73,73,54},{6,73,73,41,30}
};
static void pixel(uint8_t *p, int w, int h, int x, int y, int bright) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    size_t pos = ((size_t)x * h + h - 1 - y) * 3;
    p[pos] = bright ? 255 : 32;
    p[pos + 1] = bright ? 255 : 0;
    p[pos + 2] = bright ? 255 : 32;
}
static void label(uint8_t *p, int w, int h, int x, int y, int scale, const char *text) {
    for (; *text && x + 5 * scale < w; text++, x += 6 * scale) {
        unsigned char c = *text;
        if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        const uint8_t *glyph = c >= 'A' && c <= 'Z' ? letters[c - 'A'] :
                               c >= '0' && c <= '9' ? digits[c - '0'] : NULL;
        if (!glyph) continue;
        for (int col = 0; col < 5; col++) for (int row = 0; row < 7; row++)
            if (glyph[col] & (1u << row))
                for (int dy = 0; dy < scale; dy++) for (int dx = 0; dx < scale; dx++)
                    pixel(p, w, h, x + col * scale + dx, y + row * scale + dy, 1);
    }
}
void debug_screen_draw(uint8_t *p, size_t bytes, int w, int h, unsigned step, const char *stage) {
    if (!p || !stage || w <= 0 || h <= 0 || (size_t)w > bytes / 3 / (size_t)h) return;
    for (int x = 0; x < w; x++) for (int y = 0; y < h && y < 64; y++) pixel(p, w, h, x, y, 0);
    label(p, w, h, 8, 4, 2, "MP3DS DIAG2");
    char code[4] = { '0' + (step / 100) % 10, '0' + (step / 10) % 10, '0' + step % 10, 0 };
    label(p, w, h, 8, 26, 3, code);
    label(p, w, h, 72, 29, 1, stage);
    label(p, w, h, 72, 44, 1, "INFORME ESTA ETAPA");
}
