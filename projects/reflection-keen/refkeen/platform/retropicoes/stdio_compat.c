#include "stdio_compat.h"

#include <stdint.h>
#include <stdlib.h>

#include "compat.h"

typedef struct retropicoes_stdio_file {
    uint32_t magic;
    compat_file_t *file;
    int eof;
    int error;
} retropicoes_stdio_file_t;

enum { RETROPICOES_STDIO_MAGIC = 0x524b4649u };

static retropicoes_stdio_file_t *as_file(FILE *stream)
{
    retropicoes_stdio_file_t *file = (retropicoes_stdio_file_t *)stream;
    return file != NULL && file->magic == RETROPICOES_STDIO_MAGIC
        ? file : NULL;
}

FILE *retropicoes_fopen(const char *path, const char *mode)
{
    retropicoes_stdio_file_t *stream =
        (retropicoes_stdio_file_t *)calloc(1u, sizeof(*stream));
    if (stream == NULL)
        return NULL;
    stream->file = compat_file_open(path, mode);
    if (stream->file == NULL) {
        free(stream);
        return NULL;
    }
    stream->magic = RETROPICOES_STDIO_MAGIC;
    return (FILE *)stream;
}

int retropicoes_fclose(FILE *stream)
{
    retropicoes_stdio_file_t *file = as_file(stream);
    int result;
    if (file == NULL)
        return EOF;
    result = compat_file_close(file->file);
    file->magic = 0u;
    free(file);
    return result == 0 ? 0 : EOF;
}

size_t retropicoes_fread(void *ptr, size_t size, size_t count, FILE *stream)
{
    retropicoes_stdio_file_t *file = as_file(stream);
    size_t done;
    if (file == NULL)
        return 0u;
    done = compat_file_read(ptr, size, count, file->file);
    if (done < count)
        file->eof = 1;
    return done;
}

size_t retropicoes_fwrite(
    const void *ptr, size_t size, size_t count, FILE *stream)
{
    retropicoes_stdio_file_t *file = as_file(stream);
    size_t done;
    if (file == NULL)
        return 0u;
    done = compat_file_write(ptr, size, count, file->file);
    if (done < count)
        file->error = 1;
    return done;
}

int retropicoes_fseek(FILE *stream, long offset, int origin)
{
    retropicoes_stdio_file_t *file = as_file(stream);
    int result;
    if (file == NULL)
        return -1;
    result = compat_file_seek(file->file, offset, origin);
    if (result == 0)
        file->eof = 0;
    else
        file->error = 1;
    return result;
}

long retropicoes_ftell(FILE *stream)
{
    retropicoes_stdio_file_t *file = as_file(stream);
    long result;
    if (file == NULL)
        return -1L;
    result = compat_file_tell(file->file);
    if (result < 0)
        file->error = 1;
    return result;
}

int retropicoes_fgetc(FILE *stream)
{
    unsigned char value;
    return retropicoes_fread(&value, 1u, 1u, stream) == 1u ? value : EOF;
}

int retropicoes_fputc(int value, FILE *stream)
{
    unsigned char byte = (unsigned char)value;
    return retropicoes_fwrite(&byte, 1u, 1u, stream) == 1u ? byte : EOF;
}

char *retropicoes_fgets(char *text, int capacity, FILE *stream)
{
    retropicoes_stdio_file_t *file = as_file(stream);
    char *result;
    if (file == NULL)
        return NULL;
    result = compat_file_gets(text, capacity, file->file);
    if (result == NULL)
        file->eof = 1;
    return result;
}

int retropicoes_feof(FILE *stream)
{
    retropicoes_stdio_file_t *file = as_file(stream);
    return file != NULL ? file->eof : 0;
}

int retropicoes_ferror(FILE *stream)
{
    retropicoes_stdio_file_t *file = as_file(stream);
    return file != NULL ? file->error : 1;
}

int retropicoes_fprintf(FILE *stream, const char *format, ...)
{
    retropicoes_stdio_file_t *file = as_file(stream);
    va_list args;
    va_list measure_args;
    char *buffer;
    int length;
    size_t written;

    if (format == NULL)
        return -1;
    va_start(args, format);
    if (file == NULL) {
        length = vfprintf(stream, format, args);
        va_end(args);
        return length;
    }
    va_copy(measure_args, args);
    length = vsnprintf(NULL, 0u, format, measure_args);
    va_end(measure_args);
    if (length < 0) {
        va_end(args);
        file->error = 1;
        return -1;
    }
    buffer = (char *)malloc((size_t)length + 1u);
    if (buffer == NULL) {
        va_end(args);
        file->error = 1;
        return -1;
    }
    (void)vsnprintf(buffer, (size_t)length + 1u, format, args);
    va_end(args);
    written = compat_file_write(buffer, 1u, (size_t)length, file->file);
    free(buffer);
    if (written != (size_t)length) {
        file->error = 1;
        return -1;
    }
    return length;
}

int retropicoes_remove(const char *path)
{
    return compat_path_remove(path);
}

int retropicoes_rename(const char *old_path, const char *new_path)
{
    return retropicoes_compat_path_rename(old_path, new_path);
}
