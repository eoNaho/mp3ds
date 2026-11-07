#include "covers.h"
#include "cover.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>

static CoverTex s_slot[COVER_LRU];
static unsigned s_tick;

static Thread s_th;
static LightEvent s_wake;
static LightLock s_lock;
static volatile bool s_run;
static struct { int has; uint32_t key; char path[256]; } s_req;   /* pedido pendente (so 1: o mais recente vence) */
static uint32_t s_inflight;
static struct { int has; uint32_t key; int rc; CoverData *data; } s_done;

static void worker(void *arg) {
    (void)arg;
    while (s_run) {
        LightEvent_Wait(&s_wake);
        if (!s_run) break;
        uint32_t key; char path[256];
        LightLock_Lock(&s_lock);
        if (!s_req.has) { LightLock_Unlock(&s_lock); continue; }
        key = s_req.key; memcpy(path, s_req.path, sizeof path); s_req.has = 0; s_inflight = key;
        LightLock_Unlock(&s_lock);

        int rc = cover_ensure(COVER_DIR, key, path);
        CoverData *cd = NULL;
        if (rc == 0) { cd = malloc(sizeof *cd); if (!cd || cover_load(COVER_DIR, key, cd) != 0) { free(cd); cd = NULL; rc = 1; } }

        /* espera o main consumir o resultado anterior */
        for (;;) {
            LightLock_Lock(&s_lock);
            if (!s_done.has) { s_done.has = 1; s_done.key = key; s_done.rc = rc; s_done.data = cd; s_inflight = 0; LightLock_Unlock(&s_lock); break; }
            LightLock_Unlock(&s_lock);
            svcSleepThread(10 * 1000 * 1000);
            if (!s_run) { free(cd); return; }
        }
    }
}

void covers_init(void) {
    memset(s_slot, 0, sizeof s_slot);
    LightEvent_Init(&s_wake, RESET_ONESHOT);
    LightLock_Init(&s_lock);
    s_run = true;
    s32 prio = 0x30; svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    s_th = threadCreate(worker, NULL, 32 * 1024, prio + 1, -2, false);   /* prioridade menor que a da UI */
}

static void free_slot(CoverTex *c) {
    if (c->valid) { C3D_TexDelete(&c->tex); C3D_TexDelete(&c->mini); }
    memset(c, 0, sizeof *c);
}

void covers_exit(void) {
    s_run = false; LightEvent_Signal(&s_wake);
    if (s_th) { threadJoin(s_th, U64_MAX); threadFree(s_th); s_th = NULL; }
    if (s_done.has) free(s_done.data);
    for (int i = 0; i < COVER_LRU; i++) free_slot(&s_slot[i]);
}

/* RGBA8 (bytes R,G,B,A) -> textura tiled do PICA200. O formato RGBA8 da GPU guarda os bytes como A,B,G,R. */
static bool upload(C3D_Tex *tex, const uint8_t *rgba, int w, int h) {
    u32 *lin = (u32 *)linearAlloc((size_t)w * h * 4);
    if (!lin) return false;
    for (int i = 0; i < w * h; i++) {
        const uint8_t *p = rgba + i * 4;
        lin[i] = ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
    }
    if (!C3D_TexInit(tex, (u16)w, (u16)h, GPU_RGBA8)) { linearFree(lin); return false; }
    GSPGPU_FlushDataCache(lin, (size_t)w * h * 4);
    C3D_SyncDisplayTransfer(lin, GX_BUFFER_DIM(w, h), (u32 *)tex->data, GX_BUFFER_DIM(w, h),
        GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
        GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    linearFree(lin);
    C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    return true;
}

static CoverTex *victim(void) {
    CoverTex *v = &s_slot[0];
    for (int i = 0; i < COVER_LRU; i++) { if (!s_slot[i].key) return &s_slot[i]; if (s_slot[i].stamp < v->stamp) v = &s_slot[i]; }
    return v;
}

int covers_pump(void) {
    LightLock_Lock(&s_lock);
    int has = s_done.has; uint32_t key = s_done.key; int rc = s_done.rc; CoverData *cd = s_done.data;
    s_done.has = 0; s_done.data = NULL;
    LightLock_Unlock(&s_lock);
    if (!has) return 0;

    CoverTex *c = victim();
    free_slot(c);
    c->key = key; c->stamp = ++s_tick;
    if (rc == 0 && cd) {
        /* mini-capa 16x16 (media de blocos): vira o fundo "borrado" ao ser esticada com filtro linear */
        uint8_t mini[COVER_MINI * COVER_MINI * 4];
        const int k = COVER_SIZE / COVER_MINI;
        for (int y = 0; y < COVER_MINI; y++) for (int x = 0; x < COVER_MINI; x++) {
            unsigned s[3] = {0, 0, 0};
            for (int dy = 0; dy < k; dy++) for (int dx = 0; dx < k; dx++) {
                const uint8_t *p = cd->px + (((y * k + dy) * COVER_SIZE) + (x * k + dx)) * 4;
                s[0] += p[0]; s[1] += p[1]; s[2] += p[2];
            }
            uint8_t *d = mini + (y * COVER_MINI + x) * 4;
            d[0] = (uint8_t)(s[0] / (k * k)); d[1] = (uint8_t)(s[1] / (k * k)); d[2] = (uint8_t)(s[2] / (k * k)); d[3] = 255;
        }
        if (upload(&c->tex, cd->px, COVER_SIZE, COVER_SIZE) && upload(&c->mini, mini, COVER_MINI, COVER_MINI)) {
            c->st  = (Tex3DS_SubTexture){ COVER_SIZE, COVER_SIZE, 0.0f, 1.0f, 1.0f, 0.0f };
            c->mst = (Tex3DS_SubTexture){ COVER_MINI, COVER_MINI, 0.0f, 1.0f, 1.0f, 0.0f };
            c->img = (C2D_Image){ &c->tex, &c->st };
            c->mini_img = (C2D_Image){ &c->mini, &c->mst };
            const uint8_t *a = (const uint8_t *)&cd->accent;
            c->accent = C2D_Color32(a[0], a[1], a[2], 255);
            c->valid = 1;
        }
    }
    free(cd);
    return 1;
}

const CoverTex *covers_get(uint32_t key, const char *track_path) {
    for (int i = 0; i < COVER_LRU; i++) {
        if (s_slot[i].key == key) { s_slot[i].stamp = ++s_tick; return s_slot[i].valid ? &s_slot[i] : NULL; }
    }
    LightLock_Lock(&s_lock);
    bool busy = (s_inflight == key) || (s_done.has && s_done.key == key);
    if (!busy && !(s_req.has && s_req.key == key)) {
        s_req.has = 1; s_req.key = key;
        strncpy(s_req.path, track_path, sizeof s_req.path - 1); s_req.path[sizeof s_req.path - 1] = 0;
    }
    LightLock_Unlock(&s_lock);
    LightEvent_Signal(&s_wake);
    return NULL;
}
