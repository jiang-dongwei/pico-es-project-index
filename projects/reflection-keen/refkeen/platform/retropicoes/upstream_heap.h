#ifndef REFKEEN_UPSTREAM_HEAP_H
#define REFKEEN_UPSTREAM_HEAP_H

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "stdio_compat.h"

void *retropicoes_refkeen_game_malloc(size_t size);
void *retropicoes_refkeen_game_calloc(size_t count, size_t size);
void *retropicoes_refkeen_game_realloc(void *ptr, size_t size);
void retropicoes_refkeen_game_free(void *ptr);

#define malloc retropicoes_refkeen_game_malloc
#define calloc retropicoes_refkeen_game_calloc
#define realloc retropicoes_refkeen_game_realloc
#define free retropicoes_refkeen_game_free

#undef getc
#undef putc
#define getc retropicoes_fgetc
#define putc retropicoes_fputc

#endif
