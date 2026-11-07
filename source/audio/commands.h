#pragma once
#include <stdint.h>
#include "config.h"

enum { CMD_NONE = 0, CMD_PLAY, CMD_STOP, CMD_SEEK };
typedef struct { int cmd; char path[TRACK_PATH_MAX]; uint32_t seek; } PlaybackCommand;
/* Chamador sincroniza o acesso. Seek nao pode apagar um PLAY/STOP pendente. */
void playback_command_post(PlaybackCommand *pending, int cmd, const char *path, uint32_t seek);
