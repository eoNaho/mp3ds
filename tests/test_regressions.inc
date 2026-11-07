/* Incluido pelo runner para compartilhar CHECK e fixtures. */
#include <sys/stat.h>
#include "stream.h"

static void t_unicode_and_index(void) {
    char b[32];
    for (size_t cap = 1; cap <= 10; cap++) {
        memset(b, 0x7f, sizeof b); u_copy(b, cap, "日本語abc");
        size_t n = strlen(b);
        CHECK(n < cap && (n < 9 ? n % 3 == 0 : 1));
    }
    u_norm_ascii(b, sizeof b, "宇多田 Č Ř Ł");
    CHECK(!strcmp(b, "宇多田 č ř ł"));
    Id3Info id;
    CHECK(id3_read("fixtures/regression/utf8.mp3", &id) == 0);
    CHECK(strlen(id.title) > 96 && strlen(id.artist) > 64);
    CHECK(strlen(id.title) % 3 == 0 && strlen(id.artist) % 3 == 0 && strlen(id.album) % 3 == 0);
    Library l = {0};
    CHECK(library_scan(&l, "fixtures/regression/compilation", NULL, NULL) == 0);
    CHECK(l.count == 2 && l.tracks[0].disc == 1 && l.tracks[1].disc == 2);
    CHECK(l.tracks[0].art_key == l.tracks[1].art_key);
    CHECK(l.tracks[0].mp3.first_frame_off > 0 && l.tracks[0].mp3.sample_rate == 44100);
    CHECK(l.lookup && l.search);
    AlbumGroup albums[2]; CHECK(library_build_albums(&l, albums, 2) == 1);
    CHECK(albums[0].count == 2 && !strcmp(albums[0].artist, "Various Artists"));
    int out[4]; CHECK(library_search(&l, "宇多田", out, 4) == 1);
    CHECK(library_search(&l, "ČESKÝ řeka łódź", out, 4) == 1 && out[0] == 1);
    CHECK(library_find_path(&l, "FIXTURES/REGRESSION/COMPILATION/ONE.MP3") == 0);
    CHECK(library_find_path(&l, "moved/two.mp3") == 1);
    int ids[] = {1, 0}; library_sort_ids(&l, ids, 2, SORT_ALBUM);
    CHECK(ids[0] == 0 && ids[1] == 1);
    library_free(&l);
    CHECK(library_scan(&l, "fixtures/regression/paths", NULL, NULL) == 0);
    CHECK(l.skipped_paths == 1 && l.count == 0);
    library_free(&l);
    /* Um caminho acima do limite antigo continua valido em scan, sessao e resume. */
    char path[TRACK_PATH_MAX]; strcpy(path, "fixtures/regression/paths");
    for (int i = 0; i < 3; i++) {
        size_t off = strlen(path);
        snprintf(path + off, sizeof path - off, "/"); off++;
        for (int j = 0; j < 15; j++) { memcpy(path + off, "日本語", 9); off += 9; }
        snprintf(path + off, sizeof path - off, "%d", i);
    }
    char song[TRACK_PATH_MAX]; CHECK(strlen(path) + 11 < sizeof song); strcpy(song, path); strcat(song, "/short.mp3");
    FILE *src = fopen(M "/Sem Tag/sem_tag.mp3", "rb"), *dst = fopen(song, "wb");
    CHECK(src && dst); int c; while ((c = fgetc(src)) != EOF) fputc(c, dst); fclose(src); fclose(dst);
    CHECK(library_scan(&l, "fixtures/regression/paths", NULL, NULL) == 0);
    CHECK(l.count == 1 && strlen(l.tracks[0].path) > 256 && l.skipped_paths == 1);
    Queue q = {0}, restored = {0}; int one = 0; CHECK(queue_set(&q, &one, 1, 0) == 0);
    CHECK(session_save("fixtures/long-session.txt", &l, &q) == 0);
    CHECK(session_load("fixtures/long-session.txt", &l, &restored) == 0 && restored.count == 1);
    CHECK(resume_save("fixtures/long-resume.txt", song, 1234) == 0);
    uint32_t ms; CHECK(resume_load("fixtures/long-resume.txt", path, sizeof path, &ms) == 0);
    CHECK(!strcmp(path, song) && ms == 1234);
    remove(song); queue_free(&q); queue_free(&restored); library_free(&l);
    puts("ok unicode, album artist, discos, indice e caminhos longos");
}

static void t_legacy_migration(void) {
    typedef struct {
        char path[256], title[96], artist[64], album[64];
        uint32_t track, duration_ms, art_key, size;
        int64_t mtime, added; uint32_t plays; int64_t last_played;
    } V2;
    typedef struct {
        char path[256], title[96], artist[64], album[64];
        uint32_t track, duration_ms, art_key, size; int64_t mtime;
    } V1;
    struct { char magic[4]; uint32_t version, rec_size, count; } h = {{'M','3','D','X'}, 2, sizeof(V2), 1};
    V2 old = {0}; strcpy(old.path, "fixtures/regression/compilation/one.mp3");
    old.plays = 42; old.added = 17; old.last_played = 99;
    struct stat st; CHECK(stat(old.path, &st) == 0); old.size = st.st_size; old.mtime = st.st_mtime;
    FILE *f = fopen("fixtures/v2.idx", "wb"); CHECK(f);
    fwrite(&h, sizeof h, 1, f); fwrite(&old, sizeof old, 1, f); fclose(f);
    Library l = {0}; CHECK(library_load_index(&l, "fixtures/v2.idx") == 0);
    CHECK(l.needs_upgrade && l.tracks[0].plays == 42);
    CHECK(library_scan(&l, "fixtures/regression/compilation", NULL, NULL) == 0);
    CHECK(!l.needs_upgrade && l.last_parsed == 2 && l.tracks[0].plays == 42 && l.tracks[0].added == 17);
    CHECK(l.tracks[0].disc == 1 && l.tracks[0].mp3.sample_rate);
    CHECK(library_save_index(&l, "fixtures/v3.idx") == 0);
    Library loaded = {0}; CHECK(library_load_index(&loaded, "fixtures/v3.idx") == 0);
    CHECK(!loaded.needs_upgrade && loaded.tracks[0].plays == 42);
    f = fopen("fixtures/truncated.idx", "wb"); fwrite(&h, sizeof h, 1, f); fclose(f);
    CHECK(library_load_index(&loaded, "fixtures/truncated.idx") != 0 && loaded.count == 2);
    V1 v1 = {0}; strcpy(v1.path, old.path); v1.mtime = 22;
    h.version = 1; h.rec_size = sizeof v1;
    f = fopen("fixtures/v1.idx", "wb"); fwrite(&h, sizeof h, 1, f); fwrite(&v1, sizeof v1, 1, f); fclose(f);
    CHECK(library_load_index(&loaded, "fixtures/v1.idx") == 0 && loaded.needs_upgrade);
    CHECK(loaded.tracks[0].added == 22);
    library_free(&l); library_free(&loaded);
    puts("ok migracao v1/v2/v3 e indice truncado");
}

static void t_lookup_large(void) {
    Library l = {0}; l.count = l.cap = 6000; l.tracks = calloc(l.count, sizeof(Track)); CHECK(l.tracks);
    for (int i = 0; i < l.count; i++) snprintf(l.tracks[i].path, sizeof l.tracks[i].path, "sdmc:/Music/%d/song%d.mp3", i, i);
    strcpy(l.tracks[0].path, "sdmc:/Music/a/duplicate.mp3"); strcpy(l.tracks[1].path, "sdmc:/Music/b/duplicate.mp3");
    library_prepare(&l); CHECK(l.lookup);
    CHECK(library_find_path(&l, "other/duplicate.mp3") == -1);
    CHECK(library_find_path(&l, "sdmc:/Music/a/duplicate.mp3") == 0);
    for (int i = 2; i < l.count; i++) CHECK(library_find_path(&l, l.tracks[i].path) == i);
    Queue q = {0}, restored = {0}; int *ids = malloc(sizeof(int) * l.count); CHECK(ids);
    for (int i = 0; i < l.count; i++) ids[i] = i;
    CHECK(queue_set(&q, ids, l.count, 3000) == 0);
    CHECK(session_save("fixtures/large-session.txt", &l, &q) == 0);
    CHECK(session_load("fixtures/large-session.txt", &l, &restored) == 0);
    CHECK(restored.count == l.count && queue_current(&restored) == 3000);
    free(ids); queue_free(&q); queue_free(&restored); library_free(&l);
    puts("ok hash e sessao de 6000 faixas");
}

static void t_cover_album_refresh(void) {
    const char *dir = "fixtures/cover-regression";
    system("rm -rf fixtures/cover-regression");
    char paths[2][TRACK_PATH_MAX] = {"fixtures/regression/compilation/one.mp3", "fixtures/regression/compilation/two.mp3"};
    CoverData *cd = malloc(sizeof *cd); CHECK(cd);
    CHECK(cover_ensure_album(dir, 123, (const char (*)[TRACK_PATH_MAX])paths, 2) == 0);
    CHECK(cover_load(dir, 123, cd) == 0);
    /* A primeira faixa nao tem capa; na segunda a APIC frontal vem depois da contracapa. */
    CHECK(cd->px[2] > cd->px[0] + 100);
    struct stat st; CHECK(stat("fixtures/cover-regression/0000007b.cvr", &st) == 0);
    CHECK(st.st_size == 16 + COVER_SIZE * COVER_SIZE * 2);
    CHECK(cover_ensure(dir, 124, "fixtures/regression/missing/song.mp3") == 1);
    FILE *src = fopen(M "/Album A/folder.jpg", "rb"), *dst = fopen("fixtures/regression/missing/cover.jpg", "wb"); CHECK(src && dst);
    int c; while ((c = fgetc(src)) != EOF) fputc(c, dst); fclose(src); fclose(dst);
    CHECK(cover_clear_missing(dir) == 1);
    CHECK(cover_ensure(dir, 124, "fixtures/regression/missing/song.mp3") == 0);
    CHECK(cover_load(dir, 124, cd) == 0);
    remove("fixtures/regression/missing/cover.jpg"); free(cd);
    puts("ok capa frontal, outras faixas, RGB565 e invalidacao de CVR0");
}

static uint64_t pcm_hash(Decoder *d, long *frames) {
    int16_t buf[2048]; uint64_t hash = 1; int n; *frames = 0;
    while ((n = decoder_read(d, buf, 1024)) > 0) {
        for (int i = 0; i < n * 2; i++) hash = hash * 31 + (uint16_t)buf[i];
        *frames += n;
    }
    return hash;
}
static void t_gapless_stream(void) {
    Library l = {0}; CHECK(library_scan(&l, "fixtures/regression", NULL, NULL) == 0);
    int a = library_find_path(&l, "fixtures/regression/gapless.mp3");
    int b = library_find_path(&l, "fixtures/regression/gapless2.mp3"); CHECK(a >= 0 && b >= 0);
    const Track *t = &l.tracks[a]; CHECK(t->mp3.has_gapless && t->mp3.playable_frames == 44100);
    Decoder *d = decoder_open(t->path); CHECK(d); long total;
    uint64_t golden = pcm_hash(d, &total); CHECK(total == 44100); decoder_close(d);
    d = decoder_open_cached(t->path, &t->mp3, t->size, t->mtime); CHECK(d);
    CHECK(pcm_hash(d, &total) == golden && total == 44100); decoder_seek_ms(d, 0);
    CHECK(pcm_hash(d, &total) == golden && total == 44100); decoder_close(d);
    /* Um cache desatualizado deve reprovar size/mtime e reanalisar o arquivo. */
    Mp3Info stale = t->mp3; stale.first_frame_off = 9999;
    d = decoder_open_cached(t->path, &stale, t->size + 1, t->mtime); CHECK(d);
    CHECK(pcm_hash(d, &total) == golden && total == 44100); decoder_close(d);
    AudioStream s = {0}; CHECK(stream_start(&s, t, 0) == 0);
    stream_prepare(&s, &l.tracks[b], b); CHECK(s.next);
    int16_t buf[4096 * 2]; int n, started, transitions = 0; long counts[2] = {0}; uint64_t hashes[2] = {1, 1};
    while ((n = stream_read(&s, buf, 4096, &started)) > 0) {
        if (started >= 0) { CHECK(started == b); transitions++; }
        CHECK(transitions <= 1); counts[transitions] += n;
        for (int i = 0; i < n * 2; i++) hashes[transitions] = hashes[transitions] * 31 + (uint16_t)buf[i];
    }
    CHECK(transitions == 1 && counts[0] == 44100 && counts[1] == 44100);
    CHECK(hashes[0] == golden && hashes[1] == golden);
    stream_close(&s);
    CHECK(stream_start(&s, t, 0) == 0); stream_prepare(&s, &l.tracks[b], b);
    stream_prepare(&s, NULL, -1); CHECK(!s.next); /* edicao da fila cancela prefetch */
    stream_close(&s);
    int mono = library_find_path(&l, "fixtures/regression/missing/song.mp3"); CHECK(mono >= 0);
    CHECK(stream_start(&s, t, 0) == 0); stream_prepare(&s, &l.tracks[mono], mono);
    CHECK(s.next && decoder_sample_rate(s.next) != decoder_sample_rate(s.current));
    while (stream_read(&s, buf, 4096, &started)) CHECK(started == -1);
    CHECK(s.next); CHECK(stream_start(&s, &l.tracks[mono], 0) == 0);
    CHECK(decoder_sample_rate(s.current) == 22050); stream_close(&s); library_free(&l);
    puts("ok cache MP3, LAME sample-exato, prefetch, transicao e taxas diferentes");
}

static void t_resume_only(void) {
    SaveState *s = save_state_capture_parts(NULL, NULL, "faixa.mp3", 500, 0, 0);
    CHECK(s && !s->lib.tracks && !s->queue.items && !s->save_session && !s->save_index);
    CHECK(save_state_write(s, "fixtures/inexistente/session.txt", "fixtures/resume-only.txt", "fixtures/inexistente/index") == 0);
    save_state_free(s);
    puts("ok auto-save sem biblioteca, fila, sessao ou indice");
}
