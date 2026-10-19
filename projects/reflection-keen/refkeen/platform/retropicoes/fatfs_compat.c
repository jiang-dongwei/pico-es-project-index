#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "compat.h"
#include "be_filesystem_dir_backend.h"
#include "stdio_compat.h"
#include "ff.h"

struct compat_file { FIL handle; };
struct compat_dir {
    DIR handle;
    FILINFO info;
    char path[256];
    char name[FF_MAX_LFN + 1u];
};

static BYTE mode_flags(const char *mode, int *append)
{
    BYTE flags;
    int plus;
    if (mode == NULL || mode[0] == '\0') return 0u;
    plus = strchr(mode, '+') != NULL;
    *append = 0;
    if (mode[0] == 'r') flags = FA_READ | (plus ? FA_WRITE : 0u);
    else if (mode[0] == 'w')
        flags = FA_CREATE_ALWAYS | FA_WRITE | (plus ? FA_READ : 0u);
    else if (mode[0] == 'a') {
        flags = FA_OPEN_ALWAYS | FA_WRITE | (plus ? FA_READ : 0u);
        *append = 1;
    } else return 0u;
    return flags;
}

compat_file_t *compat_file_open(const char *path, const char *mode)
{
    compat_file_t *file;
    BYTE flags;
    int append;
    if (path == NULL || (flags = mode_flags(mode, &append)) == 0u) return NULL;
    file = (compat_file_t *)calloc(1u, sizeof(*file));
    if (file == NULL) return NULL;
    if (f_open(&file->handle, path, flags) != FR_OK) {
        free(file);
        return NULL;
    }
    if (append && f_lseek(&file->handle, f_size(&file->handle)) != FR_OK) {
        (void)f_close(&file->handle);
        free(file);
        return NULL;
    }
    return file;
}

size_t compat_file_read(
    void *ptr, size_t size, size_t count, compat_file_t *file)
{
    size_t total;
    UINT done = 0u;
    if (file == NULL || ptr == NULL || size == 0u || count == 0u ||
        size > SIZE_MAX / count) return 0u;
    total = size * count;
    if (total > UINT_MAX) return 0u;
    if (f_read(&file->handle, ptr, (UINT)total, &done) != FR_OK) return 0u;
    return done / size;
}

size_t compat_file_write(
    const void *ptr, size_t size, size_t count, compat_file_t *file)
{
    size_t total;
    UINT done = 0u;
    if (file == NULL || ptr == NULL || size == 0u || count == 0u ||
        size > SIZE_MAX / count) return 0u;
    total = size * count;
    if (total > UINT_MAX) return 0u;
    if (f_write(&file->handle, ptr, (UINT)total, &done) != FR_OK) return 0u;
    return done / size;
}

int compat_file_seek(compat_file_t *file, long offset, int origin)
{
    int64_t base;
    int64_t target;
    if (file == NULL) return -1;
    base = origin == SEEK_SET ? 0
         : origin == SEEK_CUR ? (int64_t)f_tell(&file->handle)
         : origin == SEEK_END ? (int64_t)f_size(&file->handle) : -1;
    target = base + offset;
    if (base < 0 || target < 0 || (uint64_t)target > (uint64_t)(FSIZE_t)-1)
        return -1;
    return f_lseek(&file->handle, (FSIZE_t)target) == FR_OK ? 0 : -1;
}

long compat_file_tell(compat_file_t *file)
{
    FSIZE_t pos;
    if (file == NULL) return -1L;
    pos = f_tell(&file->handle);
    return pos > LONG_MAX ? LONG_MAX : (long)pos;
}

long compat_file_size(compat_file_t *file)
{
    FSIZE_t size;
    if (file == NULL) return -1L;
    size = f_size(&file->handle);
    return size > LONG_MAX ? LONG_MAX : (long)size;
}

int compat_file_close(compat_file_t *file)
{
    int result;
    if (file == NULL) return -1;
    result = f_close(&file->handle) == FR_OK ? 0 : -1;
    free(file);
    return result;
}

int compat_file_getc(compat_file_t *file)
{
    unsigned char value;
    return compat_file_read(&value, 1u, 1u, file) == 1u ? value : EOF;
}

int compat_file_putc(int value, compat_file_t *file)
{
    unsigned char byte = (unsigned char)value;
    return compat_file_write(&byte, 1u, 1u, file) == 1u ? byte : EOF;
}

int compat_file_puts(const char *text, compat_file_t *file)
{
    size_t length = text == NULL ? 0u : strlen(text);
    return text != NULL && compat_file_write(text, 1u, length, file) == length
        ? 0 : EOF;
}

char *compat_file_gets(char *text, int capacity, compat_file_t *file)
{
    int used = 0;
    int value;
    if (text == NULL || capacity <= 1 || file == NULL) return NULL;
    while (used + 1 < capacity && (value = compat_file_getc(file)) != EOF) {
        text[used++] = (char)value;
        if (value == '\n') break;
    }
    if (used == 0) return NULL;
    text[used] = '\0';
    return text;
}

void compat_file_rewind(compat_file_t *file)
{
    if (file != NULL) (void)f_lseek(&file->handle, 0u);
}

int compat_path_exists(const char *path)
{
    FILINFO info;
    return path != NULL && f_stat(path, &info) == FR_OK;
}

compat_path_type_t compat_path_type(const char *path)
{
    FILINFO info;
    if (path == NULL || f_stat(path, &info) != FR_OK) return COMPAT_PATH_MISSING;
    return (info.fattrib & AM_DIR) ? COMPAT_PATH_DIRECTORY : COMPAT_PATH_FILE;
}

int compat_path_remove(const char *path)
{
    return path != NULL && f_unlink(path) == FR_OK ? 0 : -1;
}

int retropicoes_compat_path_rename(const char *old_path, const char *new_path)
{
    return old_path != NULL && new_path != NULL &&
           f_rename(old_path, new_path) == FR_OK ? 0 : -1;
}

int compat_dir_create(const char *path)
{
    char buffer[256];
    size_t length;
    if (path == NULL || (length = strlen(path)) == 0u || length >= sizeof(buffer))
        return -1;
    memcpy(buffer, path, length + 1u);
    for (size_t i = 1u; i <= length; ++i) {
        char saved;
        FRESULT result;
        if (buffer[i] != '/' && buffer[i] != '\\' && buffer[i] != '\0') continue;
        saved = buffer[i]; buffer[i] = '\0';
        result = f_mkdir(buffer);
        if (result == FR_EXIST) {
            FILINFO info;
            if (f_stat(buffer, &info) != FR_OK || (info.fattrib & AM_DIR) == 0u)
                return -1;
        } else if (result != FR_OK) return -1;
        buffer[i] = saved;
    }
    return 0;
}

compat_dir_t *compat_dir_open(const char *path)
{
    compat_dir_t *dir;
    size_t length;
    if (path == NULL || (length = strlen(path)) == 0u || length >= 256u) return NULL;
    dir = (compat_dir_t *)calloc(1u, sizeof(*dir));
    if (dir == NULL) return NULL;
    memcpy(dir->path, path, length + 1u);
    if (f_opendir(&dir->handle, path) != FR_OK) { free(dir); return NULL; }
    return dir;
}

const char *compat_dir_read(compat_dir_t *dir)
{
    if (dir == NULL || f_readdir(&dir->handle, &dir->info) != FR_OK ||
        dir->info.fname[0] == '\0') return NULL;
    strncpy(dir->name, dir->info.fname, sizeof(dir->name) - 1u);
    dir->name[sizeof(dir->name) - 1u] = '\0';
    return dir->name;
}

void retropicoes_compat_dir_rewind(compat_dir_t *dir)
{
    if (dir == NULL) return;
    (void)f_closedir(&dir->handle);
    (void)f_opendir(&dir->handle, dir->path);
}

void compat_dir_close(compat_dir_t *dir)
{
    if (dir == NULL) return;
    (void)f_closedir(&dir->handle);
    free(dir);
}

int retropicoes_mkdir(const char *path, int mode)
{
    (void)mode;
    return compat_dir_create(path);
}
