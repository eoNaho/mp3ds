/* Simula o FS do 3DS: rename nao substitui um destino existente. */
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
int __real_rename(const char *, const char *);
const char *test_fail_rename_to;
int __wrap_rename(const char *from, const char *to) {
    if (test_fail_rename_to && !strcmp(to, test_fail_rename_to)) {
        test_fail_rename_to = NULL; errno = EIO; return -1;
    }
    struct stat st;
    if (stat(to, &st) == 0) { errno = EEXIST; return -1; }
    return __real_rename(from, to);
}
