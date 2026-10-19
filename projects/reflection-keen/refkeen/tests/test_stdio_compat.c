#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compat.h"
#include "stdio_compat.h"
#include "upstream_heap.h"

struct compat_file {
    unsigned char bytes[512];
    size_t size;
    size_t position;
};

static struct compat_file g_storage;
static int g_renamed;

compat_file_t *compat_file_open(const char *path, const char *mode)
{
    (void)path;
    if (mode[0] == 'w') {
        g_storage.size = 0u;
        g_storage.position = 0u;
    } else if (mode[0] == 'r') {
        g_storage.position = 0u;
    }
    return &g_storage;
}

size_t compat_file_read(
    void *ptr, size_t size, size_t count, compat_file_t *file)
{
    size_t requested = size * count;
    size_t available = file->size - file->position;
    size_t copied = requested < available ? requested : available;
    memcpy(ptr, file->bytes + file->position, copied);
    file->position += copied;
    return size == 0u ? 0u : copied / size;
}

size_t compat_file_write(
    const void *ptr, size_t size, size_t count, compat_file_t *file)
{
    size_t requested = size * count;
    if (file->position + requested > sizeof(file->bytes))
        return 0u;
    memcpy(file->bytes + file->position, ptr, requested);
    file->position += requested;
    if (file->position > file->size)
        file->size = file->position;
    return count;
}

int compat_file_seek(compat_file_t *file, long offset, int origin)
{
    long base = origin == SEEK_SET ? 0L
              : origin == SEEK_CUR ? (long)file->position
                                   : (long)file->size;
    long position = base + offset;
    if (position < 0 || (size_t)position > file->size)
        return -1;
    file->position = (size_t)position;
    return 0;
}

long compat_file_tell(compat_file_t *file) { return (long)file->position; }
int compat_file_close(compat_file_t *file) { (void)file; return 0; }
char *compat_file_gets(char *text, int capacity, compat_file_t *file)
{
    int used = 0;
    while (used + 1 < capacity && file->position < file->size) {
        char value = (char)file->bytes[file->position++];
        text[used++] = value;
        if (value == '\n') break;
    }
    if (used == 0) return NULL;
    text[used] = '\0';
    return text;
}
int compat_path_remove(const char *path) { return strcmp(path, "/old") ? -1 : 0; }
int retropicoes_compat_path_rename(const char *old_path, const char *new_path)
{
    g_renamed = !strcmp(old_path, "/old") && !strcmp(new_path, "/new");
    return g_renamed ? 0 : -1;
}

static int check(int condition, const char *message)
{
    if (condition) return 0;
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    FILE *file;
    char line[32];
    int failures = 0;

    file = retropicoes_fopen("/test", "wb");
    failures += check(file != NULL, "open for write");
    failures += check(retropicoes_fprintf(file, "%s %u\n", "keen", 7u) == 7,
                      "formatted write");
    failures += check(retropicoes_fwrite("tail", 1u, 4u, file) == 4u,
                      "binary write");
    failures += check(putc('!', file) == '!', "putc bridge");
    failures += check(retropicoes_ftell(file) == 12L, "tell after writes");
    failures += check(retropicoes_fclose(file) == 0, "close writer");

    file = retropicoes_fopen("/test", "rb");
    failures += check(retropicoes_fgets(line, sizeof(line), file) == line &&
                      !strcmp(line, "keen 7\n"), "line read");
    failures += check(getc(file) == 't', "getc bridge");
    failures += check(retropicoes_fseek(file, -2L, SEEK_END) == 0,
                      "seek from end");
    failures += check(getc(file) == 'l', "last byte");
    failures += check(getc(file) == '!', "putc byte read");
    failures += check(getc(file) == EOF && retropicoes_feof(file),
                      "EOF state");
    failures += check(!retropicoes_ferror(file), "EOF is not an error");
    failures += check(retropicoes_fclose(file) == 0, "close reader");
    failures += check(retropicoes_remove("/old") == 0, "remove bridge");
    failures += check(retropicoes_rename("/old", "/new") == 0 && g_renamed,
                      "rename bridge");

    if (failures != 0)
        return 1;
    puts("REFKEEN STDIO COMPAT: PASS (16 assertions)");
    return 0;
}
