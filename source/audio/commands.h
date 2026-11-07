#pragma once
#include <stdint.h>

enum { CMD_NONE = 0, CMD_PLAY, CMD_STOP, CMD_SEEK };
typedef struct { int cmd; char path[256]; uint32_t seek; } PlaybackCommand;
/* Chamador sincroniza o acesso. Seek nao pode apagar um PLAY/STOP pendente. */
void playback_command_post(PlaybackCommand *pending, int cmd, const char *path, uint32_t seek);
