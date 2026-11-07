#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "util.h"
extern const char *test_fail_rename_to;
static const char *path = "fixtures/fileio.txt", *tmp = "fixtures/fileio.txt.tmp";
static int write_value(const char *value) {
    FILE *f = fopen(tmp, "wb"); assert(f);
    return u_commit_file(f, tmp, path, fputs(value, f) >= 0);
}
static void check_value(const char *value) {
    char text[64] = {0}; FILE *f = u_open_saved(path); assert(f);
    assert(fgets(text, sizeof text, f)); fclose(f); assert(!strcmp(text, value));
}
int main(void) {
    remove(path); remove(tmp); remove("fixtures/fileio.txt.bak");
    assert(write_value("first") == 0); assert(write_value("second") == 0); check_value("second");
    test_fail_rename_to = path;
    assert(write_value("failed") != 0 && errno == EIO); check_value("second");
    assert(rename(path, "fixtures/fileio.txt.bak") == 0); check_value("second");
    assert(write_value("recovered") == 0); check_value("recovered");
    FILE *f = fopen("/dev/full", "wb"); assert(f);
    assert(fputs("full", f) >= 0);
    assert(u_commit_file(f, tmp, path, 1) != 0 && errno == ENOSPC); check_value("recovered");
    puts("ok fileio: FAT, substituicao, rollback, backup e SD cheio");
}
