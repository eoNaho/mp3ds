/* Reproducao via ndsp. A thread de audio decodifica e mantem buffers na fila do DSP. */
#include <3ds.h>
#include <string.h>
#include <stdatomic.h>
#include "player.h"
#include "decoder.h"
#include "commands.h"
#include "stream.h"
#include "util.h"

#define CH          0
#define NBUF        4
#define BUF_FRAMES  4096                 /* ~93 ms a 44,1 kHz => ~370 ms para MP3 */
#define STACK_SZ    (48 * 1024)

enum { EQ_N = 6 };   /* off, sub, graves, voz, brilho, suave */

static ndspWaveBuf wb[NBUF];
static int16_t    *pcm[NBUF];
static uint32_t    start_frames[NBUF];   /* frames submetidos antes de cada buffer desde o (re)inicio da faixa */

static LightEvent  s_ev;
static LightLock   s_lock;
static Thread      s_th;
static volatile bool s_run;
static PlaybackCommand s_pend;
static Track s_play_track, s_next_track;
static int s_next_id = -1, s_started_id = -1;
static unsigned s_next_version;
static uint32_t s_epoch;
static _Atomic uint32_t s_visible_epoch, buffer_epoch[NBUF];
static Track s_visible_track, s_staged_track;
static int s_staged_id = -1;
static uint32_t s_staged_epoch, s_staged_duration;
static int playing_buf(u32 *pos);

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
    AudioStream stream = {0};
    bool eof = false;
    uint32_t submitted = 0;
    unsigned next_version = 0;
    while (s_run) {
        int cmd; uint32_t seek; Track play;
        LightLock_Lock(&s_lock);
        cmd = s_pend.cmd; seek = s_pend.seek;
        if (cmd == CMD_PLAY) play = s_play_track;
        s_pend.cmd = CMD_NONE;
        LightLock_Unlock(&s_lock);
        if (cmd == CMD_PLAY || cmd == CMD_STOP) {
            reset_queue();
            eof = false; submitted = 0; s_base_ms = 0; s_dur = 0; s_epoch++; s_visible_epoch = s_epoch;
            if (cmd == CMD_PLAY && stream_start(&stream, &play, seek) == 0) {
                s_hz = decoder_sample_rate(stream.current); s_dur = decoder_duration_ms(stream.current);
                s_base_ms = seek;
                LightLock_Lock(&s_lock); s_visible_track = play; s_staged_id = -1; LightLock_Unlock(&s_lock);
                ndspChnSetRate(CH, (float)s_hz);
                s_state = s_paused ? PS_PAUSED : PS_PLAYING;
            } else {
                stream_close(&stream); s_state = PS_STOPPED;
                if (cmd == CMD_PLAY) { LightLock_Lock(&s_lock); s_finished = -1; LightLock_Unlock(&s_lock); }
            }
            next_version = 0; /* revalida a proxima depois de PLAY/STOP */
        } else if (cmd == CMD_SEEK && stream.current) {
            reset_queue(); decoder_seek_ms(stream.current, seek);
            eof = false; submitted = 0; s_base_ms = seek;
        }
        if (s_paused != s_applied_pause) {
            ndspChnSetPaused(CH, s_paused); s_applied_pause = s_paused;
            if (stream.current) s_state = s_paused ? PS_PAUSED : PS_PLAYING;
        }
        /* Atualiza o prefetch antes de consumir uma proxima que foi editada. */
        Track next; int id; unsigned version;
        LightLock_Lock(&s_lock);
        version = s_next_version; id = s_next_id;
        if (version != next_version && id >= 0) next = s_next_track;
        LightLock_Unlock(&s_lock);
        if (version != next_version) {
            stream_prepare(&stream, id >= 0 ? &next : NULL, id);
            next_version = version;
            if (stream.current && stream.next && eof &&
                decoder_sample_rate(stream.current) == decoder_sample_rate(stream.next)) eof = false;
        }
        if (stream.current && !eof) {
            for (int i = 0; i < NBUF; i++) {
                if (wb[i].status != NDSP_WBUF_DONE) continue;
                int started = -1;
                int n = stream_read(&stream, pcm[i], BUF_FRAMES, &started);
                LightLock_Lock(&s_lock);
                int superseded = s_pend.cmd != CMD_NONE;
                if (!superseded && started >= 0 && version != s_next_version) {
                    /* A fila mudou enquanto decodificava: descarta a transicao antiga. */
                    s_play_track = s_visible_track;
                    playback_command_post(&s_pend, CMD_PLAY, s_play_track.path, player_pos_ms());
                    superseded = 1;
                }
                if (!superseded && started >= 0) {
                    submitted = 0; s_epoch++;
                    s_staged_id = started; s_staged_epoch = s_epoch;
                    s_staged_track = stream.next_track;
                    s_staged_duration = decoder_duration_ms(stream.current);
                    if (s_next_id == started) { s_next_id = -1; s_next_version++; }
                }
                LightLock_Unlock(&s_lock);
                if (superseded) break;
                if (n <= 0) { eof = true; break; }
                wb[i].data_vaddr = pcm[i]; wb[i].nsamples = (u32)n; wb[i].looping = false;
                start_frames[i] = submitted; buffer_epoch[i] = s_epoch; submitted += (uint32_t)n;
                DSP_FlushDataCache(pcm[i], (u32)n * 4);
                ndspChnWaveBufAdd(CH, &wb[i]);
            }
        }
        if (stream.current && eof) {
            bool idle = true;
            for (int i = 0; i < NBUF; i++) if (wb[i].status != NDSP_WBUF_DONE) idle = false;
            LightLock_Lock(&s_lock);
            if (idle && s_staged_id < 0 && s_finished != 2) { s_state = PS_STOPPED; s_finished = 1; }
            LightLock_Unlock(&s_lock);
            if (idle && s_state == PS_STOPPED) { decoder_close(stream.current); stream.current = NULL; }
        }
        LightEvent_WaitTimeout(&s_ev, 30 * 1000 * 1000);
    }
    stream_close(&stream);
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
    ndspSetCallback(NULL, NULL);
    ndspChnWaveBufClear(CH);
    /* O DSP precisa terminar antes de liberar os buffers que ele ainda pode ler. */
    ndspExit();
    s_state = PS_STOPPED;
    for (int i = 0; i < NBUF; i++) if (pcm[i]) { linearFree(pcm[i]); pcm[i] = NULL; }
}

static void post(int cmd, const char *path, uint32_t seek) {
    LightLock_Lock(&s_lock);
    if (cmd == CMD_SEEK && s_staged_id >= 0) {
        cmd = CMD_PLAY; s_play_track = s_visible_track; path = s_play_track.path;
    }
    if (cmd == CMD_PLAY || cmd == CMD_STOP) {
        s_started_id = -1; s_staged_id = -1; s_finished = 0; s_next_id = -1; s_next_version++;
    }
    playback_command_post(&s_pend, cmd, path, seek);
    LightLock_Unlock(&s_lock);
    LightEvent_Signal(&s_ev);
}

void player_play(const char *path) { player_play_seek(path, 0); }
void player_play_seek(const char *path, uint32_t start_ms) {
    Track t = {0}; u_copy(t.path, sizeof t.path, path); player_play_track(&t, start_ms);
}
void player_play_track(const Track *track, uint32_t start_ms) {
    LightLock_Lock(&s_lock);
    s_play_track = *track; s_paused = false; s_finished = 0; s_started_id = s_staged_id = -1;
    s_next_id = -1; s_next_version++;
    playback_command_post(&s_pend, CMD_PLAY, track->path, start_ms);
    s_state = PS_PLAYING;
    LightLock_Unlock(&s_lock); LightEvent_Signal(&s_ev);
}
void player_prepare_next(const Track *track, int id) {
    LightLock_Lock(&s_lock);
    if (s_staged_id >= 0) {
        if (id == s_staged_id) { LightLock_Unlock(&s_lock); return; }
        /* Ainda toca a cauda anterior; uma edicao pode substituir a faixa preparada. */
        s_play_track = s_visible_track;
        playback_command_post(&s_pend, CMD_PLAY, s_play_track.path, player_pos_ms());
        s_staged_id = -1;
    }
    if (s_next_id != id || (track && strcmp(track->path, s_next_track.path))) {
        s_next_id = id;
        if (track) s_next_track = *track;
        s_next_version++;
        LightEvent_Signal(&s_ev);
    }
    LightLock_Unlock(&s_lock);
}
int player_take_started(void) {
    LightLock_Lock(&s_lock); int id = s_started_id; s_started_id = -1; LightLock_Unlock(&s_lock); return id;
}
void player_stop(void)             { s_paused = false; post(CMD_STOP, NULL, 0); }
void player_pause(int p)           { if (s_state != PS_STOPPED) { s_paused = p ? true : false; LightEvent_Signal(&s_ev); } }
void player_seek(uint32_t ms)      { post(CMD_SEEK, NULL, ms); }
PlayerState player_state(void)     { return s_state; }
uint32_t player_duration_ms(void)  { return s_dur; }

int player_take_finished(void) {
    LightLock_Lock(&s_lock);
    if (s_staged_id >= 0) {
        u32 pos; int i = playing_buf(&pos);
        int audible = i >= 0 && buffer_epoch[i] == s_staged_epoch;
        if (i < 0) { /* faixa muito curta: a UI pode ter perdido seu primeiro buffer */
            int idle = 1, played = 0;
            for (int k = 0; k < NBUF; k++) {
                if (wb[k].status != NDSP_WBUF_DONE) idle = 0;
                if (buffer_epoch[k] == s_staged_epoch && wb[k].nsamples) played = 1;
            }
            audible = idle && played;
        }
        if (audible) {
            s_visible_epoch = s_staged_epoch; s_visible_track = s_staged_track;
            s_dur = s_staged_duration; s_base_ms = 0; s_last_frames = 0;
            s_started_id = s_staged_id; s_staged_id = -1; s_finished = 2;
        }
    }
    int f = s_finished; s_finished = 0;
    LightLock_Unlock(&s_lock); return f;
}

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
    if (i >= 0 && buffer_epoch[i] != s_visible_epoch) return 0; /* cauda da faixa anterior ainda no DSP */
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
