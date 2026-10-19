#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compat.h"
#include "ff.h"

#define COMPAT_FATFS_PATH_CAPACITY 512u

struct compat_file {
    FIL handle;
};

struct compat_dir {
    DIR handle;
    FILINFO info;
    char name[FF_MAX_LFN + 1u];
};

static BYTE mode_to_fatfs(const char *mode, int *append)
{
    BYTE flags = 0;
    int plus;

    if (mode == NULL || mode[0] == '\0') return 0;
    plus = strchr(mode, '+') != NULL;
    *append = 0;

    switch (mode[0]) {
    case 'r':
        flags = FA_READ;
        if (plus) flags |= FA_WRITE;
        break;
    case 'w':
        flags = FA_CREATE_ALWAYS | FA_WRITE;
        if (plus) flags |= FA_READ;
        break;
    case 'a':
        flags = FA_OPEN_ALWAYS | FA_WRITE;
        if (plus) flags |= FA_READ;
        *append = 1;
        break;
    default:
        return 0;
    }
    return flags;
}

compat_file_t *compat_file_open(const char *path, const char *mode)
{
    compat_file_t *file;
    BYTE flags;
    int append;

    if (path == NULL) return NULL;
    flags = mode_to_fatfs(mode, &append);
    if (flags == 0) return NULL;

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

size_t compat_file_read(void *ptr, size_t size, size_t count,
                        compat_file_t *file)
{
    size_t total;
    size_t done = 0;

    if (file == NULL || ptr == NULL || size == 0u || count == 0u ||
        size > SIZE_MAX / count) {
        return 0u;
    }
    total = size * count;
    while (done < total) {
        UINT chunk = total - done > UINT_MAX ? UINT_MAX : (UINT)(total - done);
        UINT read = 0;
        if (f_read(&file->handle, (BYTE *)ptr + done, chunk, &read) != FR_OK) {
            break;
        }
        done += read;
        if (read != chunk) break;
    }
    return done / size;
}

size_t compat_file_write(const void *ptr, size_t size, size_t count,
                         compat_file_t *file)
{
    size_t total;
    size_t done = 0;

    if (file == NULL || ptr == NULL || size == 0u || count == 0u ||
        size > SIZE_MAX / count) {
        return 0u;
    }
    total = size * count;
    while (done < total) {
        UINT chunk = total - done > UINT_MAX ? UINT_MAX : (UINT)(total - done);
        UINT written = 0;
        if (f_write(&file->handle, (const BYTE *)ptr + done, chunk,
                    &written) != FR_OK) {
            break;
        }
        done += written;
        if (written != chunk) break;
    }
    return done / size;
}

int compat_file_seek(compat_file_t *file, long offset, int origin)
{
    int64_t base;
    int64_t target;

    if (file == NULL) return -1;
    if (origin == SEEK_SET) base = 0;
    else if (origin == SEEK_CUR) base = (int64_t)f_tell(&file->handle);
    else if (origin == SEEK_END) base = (int64_t)f_size(&file->handle);
    else return -1;

    target = base + (int64_t)offset;
    if (target < 0 || (uint64_t)target > (uint64_t)(FSIZE_t)-1) return -1;
    return f_lseek(&file->handle, (FSIZE_t)target) == FR_OK ? 0 : -1;
}

long compat_file_tell(compat_file_t *file)
{
    FSIZE_t position;
    if (file == NULL) return -1;
    position = f_tell(&file->handle);
    return position > LONG_MAX ? LONG_MAX : (long)position;
}

long compat_file_size(compat_file_t *file)
{
    FSIZE_t size;
    if (file == NULL) return -1;
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
    size_t length;
    if (text == NULL) return EOF;
    length = strlen(text);
    return compat_file_write(text, 1u, length, file) == length ? 0 : EOF;
}

char *compat_file_gets(char *text, int capacity, compat_file_t *file)
{
    int used = 0;
    int value;

    if (text == NULL || capacity <= 1 || file == NULL) return NULL;
    while (used < capacity - 1) {
        value = compat_file_getc(file);
        if (value == EOF) break;
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
    if (path == NULL || f_stat(path, &info) != FR_OK) {
        return COMPAT_PATH_MISSING;
    }
    return (info.fattrib & AM_DIR) ? COMPAT_PATH_DIRECTORY : COMPAT_PATH_FILE;
}

int compat_path_remove(const char *path)
{
    return path != NULL && f_unlink(path) == FR_OK ? 0 : -1;
}

int compat_dir_create(const char *path)
{
    char buffer[COMPAT_FATFS_PATH_CAPACITY];
    size_t length;
    size_t i;

    if (path == NULL) return -1;
    length = strlen(path);
    if (length == 0u || length >= sizeof(buffer)) return -1;
    memcpy(buffer, path, length + 1u);

    for (i = 1u; i <= length; ++i) {
        FRESULT result;
        char saved;
        if (buffer[i] != '/' && buffer[i] != '\\' && buffer[i] != '\0') {
            continue;
        }
        saved = buffer[i];
        buffer[i] = '\0';
        if (buffer[0] != '\0' && !(buffer[0] == '/' && buffer[1] == '\0')) {
            result = f_mkdir(buffer);
            if (result == FR_EXIST) {
                FILINFO info;
                if (f_stat(buffer, &info) != FR_OK ||
                    (info.fattrib & AM_DIR) == 0u) {
                    return -1;
                }
            } else if (result != FR_OK) {
                return -1;
            }
        }
        buffer[i] = saved;
    }
    return 0;
}

compat_dir_t *compat_dir_open(const char *path)
{
    compat_dir_t *directory;
    if (path == NULL) return NULL;
    directory = (compat_dir_t *)calloc(1u, sizeof(*directory));
    if (directory == NULL) return NULL;
    if (f_opendir(&directory->handle, path) != FR_OK) {
        free(directory);
        return NULL;
    }
    return directory;
}

const char *compat_dir_read(compat_dir_t *directory)
{
    if (directory == NULL) return NULL;
    for (;;) {
        if (f_readdir(&directory->handle, &directory->info) != FR_OK ||
            directory->info.fname[0] == '\0') {
            return NULL;
        }
        if (strcmp(directory->info.fname, ".") == 0 ||
            strcmp(directory->info.fname, "..") == 0) {
            continue;
        }
        strncpy(directory->name, directory->info.fname,
                sizeof(directory->name) - 1u);
        directory->name[sizeof(directory->name) - 1u] = '\0';
        return directory->name;
    }
}

void compat_dir_close(compat_dir_t *directory)
{
    if (directory == NULL) return;
    (void)f_closedir(&directory->handle);
    free(directory);
}
