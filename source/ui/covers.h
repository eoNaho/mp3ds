#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <stdint.h>

typedef struct {
    uint32_t key;
    int      valid;          /* 0 = sem capa (nao tentar de novo) */
    C3D_Tex  tex, mini;
    Tex3DS_SubTexture st, mst;
    C2D_Image img, mini_img;
    uint32_t accent;         /* C2D_Color32 */
    unsigned stamp;          /* para LRU */
} CoverTex;

void covers_init(void);
void covers_exit(void);
/* Retorna a capa se ja estiver na memoria; senao agenda a leitura/criacao em thread e retorna NULL. */
const CoverTex *covers_get(uint32_t key, const char *track_path);
int covers_ready(void); /* consulta leve: resultado do worker aguardando upload */
/* Chamar depois de C3D_FrameBegin (GPU anterior terminou), antes de desenhar: sobe no maximo 1 textura pendente para a GPU. */
int  covers_pump(void);   /* 1 se subiu uma textura nova (precisa redesenhar) */
