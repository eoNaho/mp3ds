#pragma once
/* Caminhos e constantes do app. Ajuste MUSIC_ROOT se sua musica estiver em outra pasta. */
#define APP_NAME        "mp3ds"
#define DATA_DIR        "sdmc:/3ds/mp3ds"
#define MUSIC_ROOT      "sdmc:/Music"
#define INDEX_PATH      DATA_DIR "/library.idx"
#define COVER_DIR       DATA_DIR "/covers"
#define PLAYLIST_DIR    DATA_DIR "/playlists"
#define MUSIC_PLAYLIST_DIR MUSIC_ROOT "/Playlists"
#define SESSION_PATH    DATA_DIR "/session.txt"
#define RESUME_PATH     DATA_DIR "/resume.txt"

/* FLAC permanece no codigo para uma etapa futura; o caminho estavel atual e MP3. */
#define ENABLE_FLAC     0

#define COVER_SIZE      128   /* lado da capa em cache (potencia de 2) */
#define COVER_MINI      16    /* lado da mini-capa usada como fundo "borrado" */
#define COVER_LRU       6     /* capas mantidas na VRAM/RAM */
#define MAX_PLAYLISTS   48
