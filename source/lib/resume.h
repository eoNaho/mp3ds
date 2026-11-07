#pragma once
#include <stdint.h>
#include <stddef.h>

/* Estado "ultima faixa tocando / onde parou" (arquivo texto simples, apagar e recriar e seguro). */
int resume_load(const char *file, char *path, size_t cap, uint32_t *ms);  /* 0 = ok */
int resume_save(const char *file, const char *path, uint32_t ms);         /* 0 = ok; path vazio ou ms=0 apaga */
