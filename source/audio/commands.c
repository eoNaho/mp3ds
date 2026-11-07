#include "commands.h"
#include <string.h>

void playback_command_post(PlaybackCommand *pending, int cmd, const char *path, uint32_t seek) {
    if (cmd == CMD_SEEK) {
        if (pending->cmd == CMD_STOP) return;
        if (pending->cmd == CMD_PLAY) { pending->seek = seek; return; }
    }
    pending->cmd = cmd; pending->seek = seek;
    if (path) {
        strncpy(pending->path, path, sizeof pending->path - 1);
        pending->path[sizeof pending->path - 1] = 0;
    }
}
