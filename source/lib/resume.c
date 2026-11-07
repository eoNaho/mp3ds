#include "resume.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RESUME_FMT "r1"

int resume_load(const char *file, char *path, size_t cap, uint32_t *ms) {
    *ms = 0;
    if (path && cap) path[0] = 0;
    FILE *f = fopen(file, "rb");
    if (!f) return -1;
    char line[300], p[256];
    unsigned long m;
    int rc = -1;
    if (fgets(line, sizeof line, f) && fgets(p, sizeof p, f) &&
        sscanf(line, RESUME_FMT " %lu", &m) == 1) {
        p[strcspn(p, "\r\n")] = 0;
        if (path && cap) u_copy(path, cap, p);
        rc = p[0] ? 0 : -2;
        *ms = (uint32_t)m;
    }
    fclose(f);
    return rc;
}

int resume_save(const char *file, const char *path, uint32_t ms) {
    if (!path || !path[0] || ms == 0) { remove(file); return 0; }
    char tmp[300];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", file) >= (int)sizeof tmp) return -1;
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    fprintf(f, RESUME_FMT " %lu\n%s\n", (unsigned long)ms, path);
    fclose(f);
    if (rename(tmp, file) != 0) {   /* sem rename (FS sem suporte): grava direto */
        f = fopen(file, "wb");
        if (!f) { remove(tmp); return -1; }
        fprintf(f, RESUME_FMT " %lu\n%s\n", (unsigned long)ms, path);
        fclose(f);
    }
    return 0;
}
