#pragma once
#include <stdint.h>
#include <stddef.h>

void     u_copy(char *dst, size_t cap, const char *src);   /* strcpy seguro (sempre termina em \0) */
uint32_t u_fnv1a(const char *s);
int      u_mkdir_p(const char *path);                      /* 0 = ok */
void     u_utf8_trunc(char *dst, size_t cap, const char *src, int max_chars); /* corta em N caracteres + "..." */
/* minusculas, sem acentos (Latin-1), sem pontuacao: "Ñandú" -> "nandu". Para buscas. */
void     u_norm_ascii(char *dst, size_t cap, const char *src);
