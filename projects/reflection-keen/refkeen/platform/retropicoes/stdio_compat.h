#ifndef REFKEEN_RETROPICOES_STDIO_COMPAT_H
#define REFKEEN_RETROPICOES_STDIO_COMPAT_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Upstream uses stdio as its filesystem abstraction. Pico newlib is not wired
 * to the SD-card FatFS instance, so CMake remaps only ReflectionHLE translation
 * units to these ABI-compatible wrappers. libc stdout/stderr remain untouched. */
FILE *retropicoes_fopen(const char *path, const char *mode);
int retropicoes_fclose(FILE *stream);
size_t retropicoes_fread(void *ptr, size_t size, size_t count, FILE *stream);
size_t retropicoes_fwrite(
    const void *ptr, size_t size, size_t count, FILE *stream);
int retropicoes_fseek(FILE *stream, long offset, int origin);
long retropicoes_ftell(FILE *stream);
int retropicoes_fgetc(FILE *stream);
int retropicoes_fputc(int value, FILE *stream);
char *retropicoes_fgets(char *text, int capacity, FILE *stream);
int retropicoes_feof(FILE *stream);
int retropicoes_ferror(FILE *stream);
int retropicoes_fprintf(FILE *stream, const char *format, ...);
int retropicoes_remove(const char *path);
int retropicoes_rename(const char *old_path, const char *new_path);

/* FatFS has rename and rewind primitives that compat v1 does not publish. */
int retropicoes_compat_path_rename(
    const char *old_path, const char *new_path);

#ifdef __cplusplus
}
#endif

#endif
