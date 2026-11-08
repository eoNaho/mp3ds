#include <3ds.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "player.h"
#include "decoder.h"
#include "id3.h"

static pthread_mutex_t dsp_lock = PTHREAD_MUTEX_INITIALIZER;
static ndspWaveBuf *buffers[8];
static int count, paused, clears, dsp_inits, dsp_exits, fail_init;
static u32 position;
static u16 sequence;
static void (*callback)(void *);
static void *callback_arg;
static long played;
static uint64_t hash = 1;

int ndspInit(void) { dsp_inits++; return fail_init ? -1 : 0; }
void ndspExit(void) { dsp_exits++; }
void ndspSetCallback(void (*cb)(void *), void *arg) { callback = cb; callback_arg = arg; }
void ndspChnSetPaused(int ch, bool on) { (void)ch; pthread_mutex_lock(&dsp_lock); paused = on; pthread_mutex_unlock(&dsp_lock); }
void ndspChnWaveBufClear(int ch) {
    (void)ch; pthread_mutex_lock(&dsp_lock);
    for (int i = 0; i < count; i++) buffers[i]->status = NDSP_WBUF_DONE;
    count = 0; position = 0; clears++; pthread_mutex_unlock(&dsp_lock);
}
void ndspChnWaveBufAdd(int ch, ndspWaveBuf *b) {
    (void)ch; pthread_mutex_lock(&dsp_lock); assert(count < 8);
    b->sequence_id = ++sequence; b->status = count ? NDSP_WBUF_QUEUED : NDSP_WBUF_PLAYING;
    buffers[count++] = b; pthread_mutex_unlock(&dsp_lock);
}
u16 ndspChnGetWaveBufSeq(int ch) {
    (void)ch; pthread_mutex_lock(&dsp_lock); u16 id = count ? buffers[0]->sequence_id : 0;
    pthread_mutex_unlock(&dsp_lock); return id;
}
u32 ndspChnGetSamplePos(int ch) {
    (void)ch; pthread_mutex_lock(&dsp_lock); u32 pos = position;
    pthread_mutex_unlock(&dsp_lock); return pos;
}
static void consume(u32 frames) {
    pthread_mutex_lock(&dsp_lock);
    while (count && frames && !paused) {
        ndspWaveBuf *b = buffers[0]; u32 n = b->nsamples - position; if (n > frames) n = frames;
        const int16_t *pcm = b->data_vaddr;
        for (u32 i = position * 2; i < (position + n) * 2; i++) hash = hash * 31 + (uint16_t)pcm[i];
        position += n; played += n; frames -= n;
        if (position == b->nsamples) {
            b->status = NDSP_WBUF_DONE; count--; memmove(buffers, buffers + 1, count * sizeof *buffers);
            position = 0; if (count) buffers[0]->status = NDSP_WBUF_PLAYING;
        }
    }
    pthread_mutex_unlock(&dsp_lock);
    if (callback) callback(callback_arg);
}
static Track track(const char *path) {
    Track t = {0}; strcpy(t.path, path); struct stat st; assert(stat(path, &st) == 0);
    t.size = st.st_size; t.mtime = st.st_mtime;
    Id3Info id; assert(id3_read(path, &id) == 0); assert(mp3info_probe(path, id.audio_start, &t.mp3) == 0);
    return t;
}
int main(void) {
    alarm(15);
    /* Abrir e sair sem tocar nao pode inicializar/encerrar o DSP. */
    assert(player_init() == 0);
    player_eq_set(3); player_stop();
    Viz idle; player_get_viz(&idle);
    assert(player_pos_ms() == 0 && player_state() == PS_STOPPED);
    player_exit();
    assert(dsp_inits == 0 && dsp_exits == 0 && clears == 0);
    Track a = track("fixtures/regression/gapless.mp3"), b = track("fixtures/regression/gapless2.mp3");
    Decoder *d = decoder_open(a.path); assert(d); int16_t pcm[8192]; int n;
    uint64_t expected = 1;
    for (int song = 0; song < 2; song++) {
        decoder_seek_ms(d, 0);
        while ((n = decoder_read(d, pcm, 4096)) > 0)
            for (int i = 0; i < n * 2; i++) expected = expected * 31 + (uint16_t)pcm[i];
    }
    decoder_close(d);
    assert(player_init() == 0);
    fail_init = 1; player_play_track(&a, 0);
    assert(player_state() == PS_STOPPED && player_take_finished() == -2);
    player_exit(); assert(dsp_exits == 0);
    fail_init = 0;
    player_play_track(&a, 0); player_prepare_next(&b, 7);
    int started = 0, finished = 0, underruns = 0;
    for (int tick = 0; tick < 2000 && !finished; tick++) {
        usleep(4000);
        pthread_mutex_lock(&dsp_lock); int queued = count; pthread_mutex_unlock(&dsp_lock);
        if (played && played < 88200 && !queued) underruns++;
        consume(1024);
        int f = player_take_finished();
        if (f == 2) {
            assert(!started && played >= 44100); started++;
            assert(player_take_started() == 7); assert(player_duration_ms() == 1000);
            player_prepare_next(NULL, -1);
        } else if (f == 1) { assert(started == 1); finished = 1; }
        else assert(f == 0);
    }
    assert(finished && played == 88200 && hash == expected);
    assert(!underruns && clears == 1); /* Nenhum clear entre as duas faixas. */
    /* Repeat-one tambem precisa preparar novamente a mesma faixa/id. */
    long before = played; int before_clears = clears;
    player_play_track(&a, 0); player_prepare_next(&a, 3);
    int repeats = 0; finished = 0;
    for (int tick = 0; tick < 2000 && !finished; tick++) {
        usleep(4000); consume(1024);
        int f = player_take_finished();
        if (f == 2) {
            assert(player_take_started() == 3); repeats++;
            player_prepare_next(repeats < 2 ? &a : NULL, repeats < 2 ? 3 : -1);
        } else if (f == 1) finished = 1;
        else assert(f == 0);
    }
    assert(finished && repeats == 2 && played - before == 3 * 44100);
    assert(clears == before_clears + 1);
    player_exit();
    assert(dsp_inits == 2 && dsp_exits == 1);
    puts("ok player: saida sem DSP, falha de inicializacao e reproducao; DSP continuo, PCM exato e interface sincronizada com a faixa audivel");
}
