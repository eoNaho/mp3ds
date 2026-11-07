/* Testes no PC dos modulos portaveis (sem libctru). Uso: make test */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "id3.h"
#include "mp3info.h"
#include "decoder.h"
#include "viz.h"
#include "queue.h"
#include "playlist.h"
#include "resume.h"
#include "library.h"
#include "cover.h"
#include "util.h"

#define M "fixtures/music"
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FALHOU %s:%d  %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

static void t_util(void) {
    char b[32];
    u_utf8_trunc(b, sizeof b, "Canção muito longa aqui", 8);
    CHECK(!strcmp(b, "Canção m..."));
    u_utf8_trunc(b, sizeof b, "curto", 8);
    CHECK(!strcmp(b, "curto"));
    CHECK(u_fnv1a("a") != u_fnv1a("b"));
    u_norm_ascii(b, sizeof b, "Ñandú ACEÇÃO");
    CHECK(!strcmp(b, "nandu acecao"));
    u_norm_ascii(b, sizeof b, "A  B\t  c!");
    CHECK(!strcmp(b, "a b c"));
    u_norm_ascii(b, sizeof b, "42 km/h");
    CHECK(!strcmp(b, "42 km h"));
    puts("ok util");
}

/* busca no biblioteca e grupos de album */
static void t_search_albums(void) {
    Library l = {0};
    CHECK(library_scan(&l, M, NULL, NULL) == 0);
    int out[16]; int n;
    n = library_search(&l, "nandu", out, 16);            /* sem acento: acha Artista Ñandú */
    CHECK(n == 2);                                       /* 2 faixas do Album A */
    n = library_search(&l, "ÑÃNDU", out, 16);            /* com acento: mesma resposta */
    CHECK(n == 2);
    n = library_search(&l, "segunda", out, 16);
    CHECK(n == 1); CHECK(!strcmp(l.tracks[out[0]].title, "Segunda Faixa"));
    n = library_search(&l, "v1", out, 16);
    CHECK(n == 1);                                       /* Titulo V1 (ID3v1) */
    n = library_search(&l, "", out, 16);
    CHECK(n == 0);                                       /* query vazia: nada */
    n = library_search(&l, "zzzzzz", out, 16);
    CHECK(n == 0);

    /* nao achado em album sem nome: a busca casa em titulo/artista/album */
    AlbumGroup al[16]; int k = library_build_albums(&l, al, 16);
    CHECK(k >= 2);                                       /* Album A (2 faixas) e os soltos */
    int found = -1;
    for (int i = 0; i < k; i++)
        if (!strcasecmp(al[i].album, "Álbum A")) found = i;
    CHECK(found >= 0);
    CHECK(al[found].count == 2 && al[found].first + al[found].count <= l.count);
    CHECK(!strcasecmp(al[found].artist, "Artista Ñandú"));
    for (int i = 1; i < k; i++)                          /* grupos ordenados e disjuntos */
        CHECK(al[i].first >= al[i - 1].first + al[i - 1].count);
    library_free(&l);
    puts("ok search/albums");
}

/* estado de retomada ida e volta */
static void t_resume(void) {
    system("rm -rf fixtures/rz; mkdir -p fixtures/rz");
    const char *f = "fixtures/rz/resume.txt";
    char p[256]; uint32_t ms;
    CHECK(resume_load(f, p, sizeof p, &ms) != 0);        /* sem arquivo */
    CHECK(resume_save(f, "sdmc:/Musica/a.mp3", 99000) == 0);
    CHECK(resume_load(f, p, sizeof p, &ms) == 0);
    CHECK(ms == 99000 && !strcmp(p, "sdmc:/Musica/a.mp3"));
    CHECK(resume_save(f, "sdmc:/Musica/b.mp3", 123) == 0);   /* sobrescreve */
    CHECK(resume_load(f, p, sizeof p, &ms) == 0 && ms == 123 && !strcmp(p, "sdmc:/Musica/b.mp3"));
    CHECK(resume_save(f, p, 0) == 0);                    /* apaga */
    CHECK(resume_load(f, p, sizeof p, &ms) != 0);
    puts("ok resume");
}

static void t_id3(void) {
    Id3Info i;
    CHECK(id3_read(M "/Album A/01 - Cancao Teste.mp3", &i) == 0);
    CHECK(!strcmp(i.title, "Canção Teste")); CHECK(!strcmp(i.artist, "Artista Ñandú")); CHECK(!strcmp(i.album, "Álbum A"));
    CHECK(i.track == 3); CHECK(i.art_offset > 0 && i.art_size > 1000); CHECK(i.audio_start > i.art_offset);
    FILE *f = fopen(M "/Album A/01 - Cancao Teste.mp3", "rb"); uint8_t b[2];
    fseek(f, i.art_offset, SEEK_SET); fread(b, 1, 2, f); fclose(f);
    CHECK(b[0] == 0xFF && b[1] == 0xD8);                       /* capa comeca em JPEG SOI */

    CHECK(id3_read(M "/Album A/02 - Segunda.mp3", &i) == 0);   /* v2.3 + UTF-16 */
    CHECK(!strcmp(i.title, "Segunda Faixa")); CHECK(!strcmp(i.artist, "Artista Ñandú")); CHECK(i.track == 4); CHECK(i.art_offset == 0);

    CHECK(id3_read(M "/v1only.mp3", &i) == 0);                  /* fallback ID3v1 */
    CHECK(!strcmp(i.title, "Titulo V1")); CHECK(!strcmp(i.artist, "Artista V1")); CHECK(i.track == 7); CHECK(!i.has_v2);

    CHECK(id3_read(M "/Sem Tag/sem_tag.mp3", &i) == 0); CHECK(i.title[0] == 0);
    CHECK(id3_read(M "/nao_existe.mp3", &i) == -1);
    puts("ok id3");
}

static void t_mp3info(void) {
    Id3Info id; Mp3Info mi;
    id3_read(M "/Album A/01 - Cancao Teste.mp3", &id);
    CHECK(mp3info_probe(M "/Album A/01 - Cancao Teste.mp3", id.audio_start, &mi) == 0);
    CHECK(mi.sample_rate == 44100 && mi.channels == 2 && mi.bitrate_kbps == 128);
    CHECK(mi.duration_ms > 1900 && mi.duration_ms < 2150);
    id3_read(M "/Sem Tag/sem_tag.mp3", &id);
    CHECK(mp3info_probe(M "/Sem Tag/sem_tag.mp3", id.audio_start, &mi) == 0);
    CHECK(mi.sample_rate == 22050 && mi.channels == 1);
    CHECK(mp3info_probe(M "/quebrado.mp3", 0, &mi) != 0);
    puts("ok mp3info");
}

static void t_decoder(void) {
    Decoder *d = decoder_open(M "/Album A/01 - Cancao Teste.mp3");
    CHECK(d); CHECK(decoder_sample_rate(d) == 44100);
    static int16_t buf[4096 * 2];
    long total = 0; int n; int16_t first[1024];
    int got_first = 0;
    while ((n = decoder_read(d, buf, 4096)) > 0) {
        if (!got_first) { memcpy(first, buf, sizeof first); got_first = 1; }
        total += n;
    }
    CHECK(total > 88200 - 4608 && total < 88200 + 4608);       /* ~2 s */
    double e = 0; for (int i = 0; i < 1024; i++) e += fabs((double)first[i]); CHECK(e > 1000);

    decoder_seek_ms(d, 1000);
    long after = 0; while ((n = decoder_read(d, buf, 4096)) > 0) after += n;
    CHECK(after > 38000 && after < 52000);                      /* ~1 s restante */
    decoder_close(d);

    d = decoder_open(M "/Sem Tag/sem_tag.mp3");                  /* mono -> estereo */
    CHECK(d && decoder_sample_rate(d) == 22050);
    n = decoder_read(d, buf, 1024); CHECK(n == 1024);
    for (int i = 0; i < 1024; i++) CHECK(buf[2 * i] == buf[2 * i + 1]);
    decoder_close(d);
    CHECK(decoder_open(M "/quebrado.mp3") == NULL);
    puts("ok decoder");
}

/* seek pela tabela Xing TOC (fixture com frame-info manual; CBR linear) */
static void t_decoder_xing(void) {
    Id3Info id; Mp3Info mi;
    CHECK(id3_read(M "/xingtoc.mp3", &id) == 0);
    CHECK(mp3info_probe(M "/xingtoc.mp3", id.audio_start, &mi) == 0);
    CHECK(mi.has_toc == 1);
    CHECK(mi.duration_ms > 1800 && mi.duration_ms < 2300);
    Decoder *d = decoder_open(M "/xingtoc.mp3");
    CHECK(d); CHECK(decoder_sample_rate(d) == 44100);
    static int16_t buf[4096 * 2];
    long after = 0, n; double secs;
    decoder_seek_ms(d, mi.duration_ms / 2);
    while ((n = decoder_read(d, buf, 4096)) > 0) after += n;
    secs = after / (double)mi.sample_rate;
    CHECK(secs > 0.70 && secs < 1.30);                          /* ~metade da duracao */
    decoder_seek_ms(d, 0);
    long total = 0; while ((n = decoder_read(d, buf, 4096)) > 0) total += n;
    secs = total / (double)mi.sample_rate;
    CHECK(secs > 1.9 && secs < 2.35);                           /* faixa inteira */
    decoder_close(d);
    puts("ok decoder xing");
}

static void t_viz(void) {
    Decoder *d = decoder_open(M "/Album A/01 - Cancao Teste.mp3");
    static int16_t buf[2048 * 2];
    decoder_read(d, buf, 1024);                                  /* descarta atraso do encoder */
    int n = decoder_read(d, buf, 512); CHECK(n == 512);
    Viz v; viz_init(&v); viz_feed(&v, buf, 512, 44100);
    int best = 0; for (int i = 1; i < VIZ_BANDS; i++) if (v.level[i] > v.level[best]) best = i;
    CHECK(best == 3);                                            /* 1 kHz -> banda 3 */
    CHECK(v.level[3] > 0.5f); CHECK(v.level[0] < v.level[3] - 0.2f); CHECK(v.level[7] < v.level[3] - 0.2f);
    float before = v.level[3]; viz_decay(&v); CHECK(v.level[3] < before);
    decoder_close(d);
    puts("ok viz");
}

static void t_queue(void) {
    Queue q; queue_init(&q, 7);
    int ids[5] = {10, 11, 12, 13, 14};
    queue_set(&q, ids, 5, 2);
    CHECK(queue_current(&q) == 12); CHECK(queue_next(&q, 0) == 13); CHECK(queue_next(&q, 0) == 14); CHECK(queue_next(&q, 0) == -1);
    q.repeat = REPEAT_ALL; CHECK(queue_next(&q, 0) == 10); CHECK(queue_prev(&q) == 14);
    q.repeat = REPEAT_ONE; CHECK(queue_next(&q, 0) == 14); CHECK(queue_next(&q, 1) == 10);
    q.repeat = REPEAT_OFF; queue_set(&q, ids, 5, 1);
    queue_set_shuffle(&q, 1);
    CHECK(queue_current(&q) == 11);                              /* shuffle mantem a faixa atual */
    int seen[5] = {0}, cur = queue_current(&q); seen[cur - 10]++;
    for (int i = 0; i < 4; i++) { cur = queue_next(&q, 0); CHECK(cur >= 10); seen[cur - 10]++; }
    for (int i = 0; i < 5; i++) CHECK(seen[i] == 1);             /* cada faixa exatamente uma vez */
    CHECK(queue_next(&q, 0) == -1);
    int cur2 = queue_current(&q); queue_set_shuffle(&q, 0); CHECK(queue_current(&q) == cur2);
    queue_free(&q);
    puts("ok queue");
}

static void t_playlist(void) {
    system("mkdir -p fixtures/pl");
    char in[3][PL_PATH_MAX] = {"sdmc:/Music/a.mp3", "sdmc:/Music/Album/b ç.mp3", "sdmc:/Music/c.mp3"};
    CHECK(playlist_save("fixtures/pl/Minha.m3u", (const char (*)[PL_PATH_MAX])in, 3) == 0);
    char out[8][PL_PATH_MAX];
    CHECK(playlist_load("fixtures/pl/Minha.m3u", out, 8) == 3);
    for (int i = 0; i < 3; i++) CHECK(!strcmp(in[i], out[i]));

    FILE *f = fopen("fixtures/pl/win.m3u8", "wb");              /* BOM + CRLF + relativo + barra invertida + /abs */
    fputs("\xEF\xBB\xBF#EXTM3U\r\n#EXTINF:1,x\r\nAlbum\\um.mp3\r\n/Music/dois.mp3\r\n\r\n", f); fclose(f);
    CHECK(playlist_load("fixtures/pl/win.m3u8", out, 8) == 2);
    CHECK(!strcmp(out[0], "fixtures/pl/Album/um.mp3")); CHECK(!strcmp(out[1], "sdmc:/Music/dois.mp3"));

    PlaylistInfo pi[8]; int n = playlist_scan("fixtures/pl", pi, 8);
    CHECK(n == 2); CHECK(!strcmp(pi[0].name, "Minha")); CHECK(!strcmp(pi[1].name, "win"));
    puts("ok playlist");
}

static int g_progress;
static int on_prog(int found, const char *cur, void *ud) { (void)cur; (void)ud; g_progress = found; return 0; }

static void t_library(void) {
    Library l = {0};
    CHECK(library_scan(&l, M, on_prog, NULL) == 0);
    CHECK(l.count == 5); CHECK(g_progress == 5); CHECK(l.last_parsed == 5);   /* quebrado.mp3 e .txt ficam de fora */
    CHECK(!strcasecmp(l.tracks[0].artist, "Artista Ñandú") || l.tracks[0].artist[0]);
    int i = -1; for (int k = 0; k < l.count; k++) if (!strcmp(l.tracks[k].title, "Canção Teste")) i = k;
    CHECK(i >= 0); CHECK(l.tracks[i].track == 3); CHECK(l.tracks[i].duration_ms > 1900);
    int j = -1; for (int k = 0; k < l.count; k++) if (!strcmp(l.tracks[k].title, "sem_tag")) j = k;
    CHECK(j >= 0); CHECK(!strcmp(l.tracks[j].artist, "Desconhecido"));
    /* mesma album+artista => mesma chave de capa */
    int s = -1; for (int k = 0; k < l.count; k++) if (!strcmp(l.tracks[k].title, "Segunda Faixa")) s = k;
    CHECK(l.tracks[s].art_key == l.tracks[i].art_key);
    /* ordenacao: artista, album, faixa */
    CHECK(l.tracks[i].track < l.tracks[s].track && s == i + 1);

    CHECK(library_save_index(&l, "fixtures/lib.idx") == 0);
    Library l2 = {0}; CHECK(library_load_index(&l2, "fixtures/lib.idx") == 0);
    CHECK(l2.count == l.count && !memcmp(l2.tracks, l.tracks, sizeof(Track) * l.count));
    CHECK(library_scan(&l2, M, NULL, NULL) == 0);
    CHECK(l2.count == 5 && l2.last_parsed == 0);                 /* incremental: nada releu */
    CHECK(library_find_path(&l2, l.tracks[i].path) >= 0); CHECK(library_find_path(&l2, "nao/existe") == -1);
    library_free(&l); library_free(&l2);
    puts("ok library");
}

static void t_cover(void) {
    const char *cd = "fixtures/covers";
    system("rm -rf fixtures/covers");
    CoverData *c = malloc(sizeof *c);
    CHECK(cover_ensure(cd, 1, M "/Album A/01 - Cancao Teste.mp3") == 0);          /* embutida (vermelha) */
    CHECK(cover_load(cd, 1, c) == 0);
    uint8_t *a = (uint8_t *)&c->accent; CHECK(a[0] > a[1] + 40 && a[0] > a[2] + 40);
    CHECK(c->px[3] == 255);
    CHECK(cover_ensure(cd, 2, M "/Album A/02 - Segunda.mp3") == 0);               /* folder.jpg (azul) */
    CHECK(cover_load(cd, 2, c) == 0); a = (uint8_t *)&c->accent; CHECK(a[2] > a[0] + 40);
    CHECK(cover_ensure(cd, 3, M "/Sem Tag/sem_tag.mp3") == 1);                    /* sem capa */
    CHECK(cover_load(cd, 3, c) == 1);
    CHECK(cover_ensure(cd, 3, M "/Sem Tag/sem_tag.mp3") == 1);                    /* marcador reaproveitado */
    free(c);
    puts("ok cover");
}

int main(void) {
    t_util(); t_id3(); t_mp3info(); t_decoder(); t_decoder_xing(); t_viz(); t_queue(); t_playlist(); t_library(); t_search_albums(); t_resume(); t_cover();
    puts("TODOS OS TESTES PASSARAM");
    return 0;
}
