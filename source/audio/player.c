/* Reproducao via ndsp. A thread de audio decodifica e mantem buffers na fila do DSP. */
#include <3ds.h>
#include <string.h>
#include "player.h"
#include "decoder.h"

#define CH          0
#define NBUF        4
#define BUF_FRAMES  4096                 /* ~93 ms a 44,1 kHz => ~370 ms para MP3 */
#define STACK_SZ    (48 * 1024)

enum { CMD_NONE = 0, CMD_PLAY, CMD_STOP, CMD_SEEK };
enum { EQ_N = 6 };   /* off, sub, graves, voz, brilho, suave */

static ndspWaveBuf wb[NBUF];
static int16_t    *pcm[NBUF];
static uint32_t    start_frames[NBUF];   /* frames submetidos antes de cada buffer desde o (re)inicio da faixa */

static LightEvent  s_ev;
static LightLock   s_lock;
static Thread      s_th;
static volatile bool s_run;
static struct { int cmd; char path[256]; uint32_t seek; } s_pend;

static volatile PlayerState s_state = PS_STOPPED;
static volatile bool     s_paused, s_applied_pause;
static volatile int      s_finished;      /* 0, 1 = fim, -1 = erro */
static volatile uint32_t s_dur, s_base_ms, s_hz = 44100, s_last_frames;
static Viz s_viz;

typedef struct { const char *name; float f0, q, gain; } EqPreset;

/* equalizador: pico IIR do DSP (gain bruto = 10^(dB/40); <1 corta) */
static int s_eq_preset;
static const EqPreset s_eq[EQ_N] = {
    { "Desligado",     0.0f, 1.0f, 1.0f    },
    { "Sub"     ,     60.0f, 1.0f, 2.5119f },   /* +9 dB em 60 Hz   */
    { "Graves"  ,    100.0f, 1.2f, 1.4125f },   /* +6 dB em 100 Hz  */
    { "Voz"     ,   2500.0f, 1.2f, 1.2589f },   /* +4 dB em 2,5 kHz */
    { "Brilho"  ,  10000.0f, 1.2f, 1.2589f },   /* +4 dB em 10 kHz  */
    { "Suave"   ,   3000.0f, 1.0f, 0.6310f },   /* -4 dB em 3 kHz, corta aspereza */
};

void player_eq_apply(void);

static void on_frame(void *unused) { (void)unused; LightEvent_Signal(&s_ev); }

static void reset_queue(void) {
    ndspChnWaveBufClear(CH);
    for (int i = 0; i < NBUF; i++) wb[i].status = NDSP_WBUF_DONE;
    s_last_frames = 0;
}

static void audio_thread(void *arg) {
    (void)arg;
    Decoder *dec = NULL;
    bool eof = false;
    uint32_t submitted = 0;

    while (s_run) {
        int cmd; char path[256] = ""; uint32_t seek;
        LightLock_Lock(&s_lock);
        cmd = s_pend.cmd; seek = s_pend.seek;
        if (cmd == CMD_PLAY) { memcpy(path, s_pend.path, sizeof path); }
        s_pend.cmd = CMD_NONE;
        LightLock_Unlock(&s_lock);

        if (cmd == CMD_PLAY || cmd == CMD_STOP) {
            reset_queue();
            if (dec) { decoder_close(dec); dec = NULL; }
            eof = false; submitted = 0; s_base_ms = 0; s_last_frames = 0; s_dur = 0;
            if (cmd == CMD_PLAY) {
                dec = decoder_open(path);
                if (dec) {
                    s_hz = (uint32_t)decoder_sample_rate(dec);
                    s_dur = decoder_duration_ms(dec);
                    if (seek) {                              /* retomada: abre e salta no mesmo comando */
                        decoder_seek_ms(dec, seek);
                        s_base_ms = seek;
                    }
                    ndspChnSetRate(CH, (float)s_hz);
                    s_state = PS_PLAYING;
                } else { s_state = PS_STOPPED; s_finished = -1; }
            } else s_state = PS_STOPPED;
        } else if (cmd == CMD_SEEK && dec) {
            reset_queue();
            decoder_seek_ms(dec, seek);
            eof = false; submitted = 0; s_base_ms = seek;
        }

        if (s_paused != s_applied_pause) {
            ndspChnSetPaused(CH, s_paused);
            s_applied_pause = s_paused;
            if (dec) s_state = s_paused ? PS_PAUSED : PS_PLAYING;
        }

        if (dec && !eof) {
            for (int i = 0; i < NBUF; i++) {
                if (wb[i].status != NDSP_WBUF_DONE) continue;
                int n = decoder_read(dec, pcm[i], BUF_FRAMES);
                if (n <= 0) { eof = true; break; }
                wb[i].data_vaddr = pcm[i];
                wb[i].nsamples = (u32)n;
                wb[i].looping = false;
                start_frames[i] = submitted; submitted += (uint32_t)n;
                DSP_FlushDataCache(pcm[i], (u32)n * 4);
                ndspChnWaveBufAdd(CH, &wb[i]);
            }
        }
        if (dec && eof) {                       /* acabou de decodificar: espera o DSP esvaziar a fila */
            bool idle = true;
            for (int i = 0; i < NBUF; i++) if (wb[i].status != NDSP_WBUF_DONE) idle = false;
            if (idle) { decoder_close(dec); dec = NULL; s_state = PS_STOPPED; s_finished = 1; }
        }
        LightEvent_WaitTimeout(&s_ev, 30 * 1000 * 1000);   /* acorda no callback do ndsp ou em 30 ms */
    }
    if (dec) decoder_close(dec);
}

int player_init(void) {
    if (R_FAILED(ndspInit())) return -1;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(CH);
    ndspChnSetInterp(CH, NDSP_INTERP_LINEAR);
    ndspChnSetFormat(CH, NDSP_FORMAT_STEREO_PCM16);
    ndspChnSetRate(CH, 44100.0f);
    float mix[12] = {0}; mix[0] = mix[1] = 1.0f;
    ndspChnSetMix(CH, mix);
    for (int i = 0; i < NBUF; i++) {
        pcm[i] = (int16_t *)linearAlloc(BUF_FRAMES * 4);
        memset(&wb[i], 0, sizeof wb[i]);
        wb[i].status = NDSP_WBUF_DONE;
    }
    LightEvent_Init(&s_ev, RESET_ONESHOT);
    LightLock_Init(&s_lock);
    viz_init(&s_viz);
    player_eq_apply();
    ndspSetCallback(on_frame, NULL);

    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    prio = (prio < 0x1A) ? 0x18 : prio - 2;
    s_run = true;
    APT_SetAppCpuTimeLimit(70);                                  /* audio/UI com folga; o 3DS pode reduzir o clock em repouso */
    s_th = threadCreate(audio_thread, NULL, STACK_SZ, prio, 1, false);
    if (!s_th) s_th = threadCreate(audio_thread, NULL, STACK_SZ, prio, -1, false);
    return s_th ? 0 : -2;
}

void player_exit(void) {
    s_run = false;
    LightEvent_Signal(&s_ev);
    if (s_th) { threadJoin(s_th, U64_MAX); threadFree(s_th); s_th = NULL; }
    ndspChnWaveBufClear(CH);
    for (int i = 0; i < NBUF; i++) if (pcm[i]) { linearFree(pcm[i]); pcm[i] = NULL; }
    ndspExit();
}

static void post(int cmd, const char *path, uint32_t seek) {
    LightLock_Lock(&s_lock);
    s_pend.cmd = cmd; s_pend.seek = seek;
    if (path) { strncpy(s_pend.path, path, sizeof s_pend.path - 1); s_pend.path[sizeof s_pend.path - 1] = 0; }
    LightLock_Unlock(&s_lock);
    LightEvent_Signal(&s_ev);
}

void player_play(const char *path) { player_play_seek(path, 0); }
void player_play_seek(const char *path, uint32_t start_ms) { s_paused = false; s_finished = 0; s_state = PS_PLAYING; post(CMD_PLAY, path, start_ms); }
void player_stop(void)             { s_paused = false; post(CMD_STOP, NULL, 0); }
void player_pause(int p)           { if (s_state != PS_STOPPED) { s_paused = p ? true : false; LightEvent_Signal(&s_ev); } }
void player_seek(uint32_t ms)      { post(CMD_SEEK, NULL, ms); }
PlayerState player_state(void)     { return s_state; }
uint32_t player_duration_ms(void)  { return s_dur; }

int player_take_finished(void) { int f = s_finished; s_finished = 0; return f; }

/* acha o buffer que o DSP esta tocando e a posicao (em amostras) dentro dele */
static int playing_buf(u32 *pos) {
    u16 seq = ndspChnGetWaveBufSeq(CH);
    for (int i = 0; i < NBUF; i++) {
        if (wb[i].status == NDSP_WBUF_PLAYING && wb[i].sequence_id == seq) {
            u32 sp = ndspChnGetSamplePos(CH);
            *pos = sp < wb[i].nsamples ? sp : wb[i].nsamples;
            return i;
        }
    }
    return -1;
}

uint32_t player_pos_ms(void) {
    if (s_state == PS_STOPPED) return 0;
    u32 sp; int i = playing_buf(&sp);
    if (i >= 0) s_last_frames = start_frames[i] + sp;
    return s_base_ms + (uint32_t)((uint64_t)s_last_frames * 1000 / (s_hz ? s_hz : 44100));
}

void player_get_viz(Viz *out) {
    u32 sp;
    int i = (s_state == PS_PLAYING) ? playing_buf(&sp) : -1;
    if (i >= 0 && wb[i].nsamples > sp) viz_feed(&s_viz, pcm[i] + 2 * sp, (int)(wb[i].nsamples - sp), (int)s_hz);
    else viz_decay(&s_viz);
    *out = s_viz;
}

void player_eq_apply(void) {
    if (s_eq_preset <= 0 || s_eq_preset >= EQ_N) { ndspChnIirBiquadSetEnable(CH, false); return; }
    const EqPreset *p = &s_eq[s_eq_preset];
    ndspChnIirBiquadSetParamsPeakingEqualizer(CH, p->f0, p->q, p->gain);
    ndspChnIirBiquadSetEnable(CH, true);
}

void player_eq_set(int preset) {
    s_eq_preset = (preset < 0 || preset >= EQ_N) ? 0 : preset;
    player_eq_apply();
}
void player_eq_cycle(void) { s_eq_preset = (s_eq_preset + 1) % EQ_N; player_eq_apply(); }
int player_eq_get(void) { return s_eq_preset; }
int player_eq_count(void) { return EQ_N; }
const char *player_eq_name(int preset) { return s_eq[(preset < 0 || preset >= EQ_N) ? 0 : preset].name; }
