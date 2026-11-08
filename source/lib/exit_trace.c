#include "exit_trace.h"
#ifdef __3DS__
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#endif

/* Log silencioso de arranque/encerramento no SD. */
static int active;      /* registra etapas a partir de exit_trace_open */
static int began;
static unsigned step;
static ExitTraceSink output;

void exit_trace_set_sink(ExitTraceSink sink) { output = sink; }

void exit_trace_open(void) {
#ifdef __3DS__
    FILE *lf = fopen(DATA_DIR "/exit.log", "w");
    if (lf) fclose(lf);
    active = 1;
#endif
}

void exit_trace_begin(void) {
    if (!active) active = 1;
    if (began) return;
    began = 1;
    exit_trace("exit: closing");
}

void exit_trace(const char *stage) {
    if (!active) return;
    step++;
#ifdef __3DS__
    svcOutputDebugString(stage, strlen(stage));
    FILE *lf = fopen(DATA_DIR "/exit.log", "a");
    if (lf) {
        fprintf(lf, "%u %llu ms %s\n", step, (unsigned long long)svcGetSystemTick() / (SYSCLOCK_ARM11 / 1000), stage);
        fclose(lf);
    }
#else
    if (output) output(step, stage);
#endif
}
