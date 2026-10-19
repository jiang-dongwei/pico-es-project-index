/*
 * Host (libc) implementation of the compat v1 platform boundary for S17-A.
 *
 * This backend exists so the pinned noiz2sa engine can be compiled, linked
 * and smoke-run headlessly on the Windows host before the HW_CONFIG=16
 * backend lands. It implements every platform function declared in compat.h
 * over the C standard library; the framebuffer is heap memory and present()
 * is a no-op.
 *
 * The HW16 target backend (FatFS/HSTX/HDMI) is a separate file; this one must
 * never be linked into a Pico build.
 */
#include "compat.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#include <time.h>
#endif

struct compat_file {
    FILE *stream;
};

struct compat_dir {
    DIR *handle;
};

static compat_pixel_t g_framebuffer_pixels[COMPAT_SCREEN_WIDTH *
                                           COMPAT_SCREEN_HEIGHT];
static compat_framebuffer_t g_framebuffer = {
    g_framebuffer_pixels,
    COMPAT_SCREEN_WIDTH,
    COMPAT_SCREEN_WIDTH,
    COMPAT_SCREEN_HEIGHT,
};

#ifdef _WIN32
static LARGE_INTEGER g_qpc_frequency;
static LARGE_INTEGER g_qpc_start;

static void host_clock_start(void)
{
    QueryPerformanceFrequency(&g_qpc_frequency);
    QueryPerformanceCounter(&g_qpc_start);
}

static uint64_t host_elapsed_us(void)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart - g_qpc_start.QuadPart) * 1000000ULL /
                      g_qpc_frequency.QuadPart);
}
#else
static struct timespec g_start_time;

static void host_clock_start_posix(void)
{
    clock_gettime(CLOCK_MONOTONIC, &g_start_time);
}

static uint64_t host_elapsed_us_posix(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return ((uint64_t)(now.tv_sec - g_start_time.tv_sec) * 1000000ULL) +
           (uint64_t)(now.tv_nsec - g_start_time.tv_nsec) / 1000ULL;
}
#endif

int compat_init(void)
{
#ifdef _WIN32
    host_clock_start();
#else
    host_clock_start_posix();
#endif
    memset(g_framebuffer_pixels, 0, sizeof(g_framebuffer_pixels));
    return 0;
}

void compat_shutdown(void)
{
}

compat_framebuffer_t *compat_framebuffer(void)
{
    return &g_framebuffer;
}

void compat_present(void)
{
}

void compat_poll_input(void)
{
}

int compat_key_down(compat_key_t key)
{
    (void)key;
    return 0;
}

int compat_key_pressed(compat_key_t key)
{
    (void)key;
    return 0;
}

int compat_audio_start(unsigned sample_rate, unsigned channels,
                       compat_audio_callback_t callback, void *user_data)
{
    (void)sample_rate;
    (void)channels;
    (void)callback;
    (void)user_data;
    return 0;
}

void compat_audio_stop(void)
{
}

compat_file_t *compat_file_open(const char *path, const char *mode)
{
    compat_file_t *file = (compat_file_t *)malloc(sizeof(compat_file_t));
    if (file == NULL) {
        return NULL;
    }
    file->stream = fopen(path, mode);
    if (file->stream == NULL) {
        free(file);
        return NULL;
    }
    return file;
}

size_t compat_file_read(void *ptr, size_t size, size_t count,
                        compat_file_t *file)
{
    if (file == NULL) {
        return 0;
    }
    return fread(ptr, size, count, file->stream);
}

size_t compat_file_write(const void *ptr, size_t size, size_t count,
                         compat_file_t *file)
{
    if (file == NULL) {
        return 0;
    }
    return fwrite(ptr, size, count, file->stream);
}

int compat_file_seek(compat_file_t *file, long offset, int origin)
{
    if (file == NULL) {
        return -1;
    }
    return fseek(file->stream, offset, origin);
}

long compat_file_tell(compat_file_t *file)
{
    if (file == NULL) {
        return -1L;
    }
    return ftell(file->stream);
}

long compat_file_size(compat_file_t *file)
{
    long current;
    long size;
    if (file == NULL) {
        return -1L;
    }
    current = ftell(file->stream);
    if (current < 0L || fseek(file->stream, 0, SEEK_END) != 0) {
        return -1L;
    }
    size = ftell(file->stream);
    if (fseek(file->stream, current, SEEK_SET) != 0) {
        return -1L;
    }
    return size;
}

int compat_file_close(compat_file_t *file)
{
    int rc = -1;
    if (file != NULL) {
        if (file->stream != NULL) {
            rc = fclose(file->stream);
        }
        free(file);
    }
    return rc;
}

int compat_file_getc(compat_file_t *file)
{
    if (file == NULL) {
        return -1;
    }
    return fgetc(file->stream);
}

int compat_file_putc(int value, compat_file_t *file)
{
    if (file == NULL) {
        return -1;
    }
    return fputc(value, file->stream);
}

int compat_file_puts(const char *text, compat_file_t *file)
{
    if (file == NULL) {
        return -1;
    }
    return fputs(text, file->stream);
}

char *compat_file_gets(char *text, int capacity, compat_file_t *file)
{
    if (file == NULL) {
        return NULL;
    }
    return fgets(text, capacity, file->stream);
}

void compat_file_rewind(compat_file_t *file)
{
    if (file != NULL) {
        rewind(file->stream);
    }
}

int compat_path_exists(const char *path)
{
    FILE *probe = fopen(path, "rb");
    if (probe != NULL) {
        fclose(probe);
        return 1;
    }
    {
        DIR *dir = opendir(path);
        if (dir != NULL) {
            closedir(dir);
            return 1;
        }
    }
    return 0;
}

compat_path_type_t compat_path_type(const char *path)
{
    DIR *dir = opendir(path);
    if (dir != NULL) {
        closedir(dir);
        return COMPAT_PATH_DIRECTORY;
    }
    {
        FILE *probe = fopen(path, "rb");
        if (probe != NULL) {
            fclose(probe);
            return COMPAT_PATH_FILE;
        }
    }
    return COMPAT_PATH_MISSING;
}

int compat_path_remove(const char *path)
{
    return remove(path) == 0 ? 1 : 0;
}

int compat_dir_create(const char *path)
{
    /* Host helper only; single-level creation is enough for tests. */
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST ? 1 : 0;
#else
    return mkdir(path, 0777) == 0 ? 1 : 0;
#endif
}

compat_dir_t *compat_dir_open(const char *path)
{
    compat_dir_t *dir = (compat_dir_t *)malloc(sizeof(compat_dir_t));
    if (dir == NULL) {
        return NULL;
    }
    dir->handle = opendir(path);
    if (dir->handle == NULL) {
        free(dir);
        return NULL;
    }
    return dir;
}

const char *compat_dir_read(compat_dir_t *dir)
{
    struct dirent *entry;
    if (dir == NULL || dir->handle == NULL) {
        return NULL;
    }
    entry = readdir(dir->handle);
    return entry != NULL ? entry->d_name : NULL;
}

void compat_dir_close(compat_dir_t *dir)
{
    if (dir != NULL) {
        if (dir->handle != NULL) {
            closedir(dir->handle);
        }
        free(dir);
    }
}

uint32_t compat_ticks_ms(void)
{
#ifdef _WIN32
    return (uint32_t)(host_elapsed_us() / 1000ULL);
#else
    return (uint32_t)(host_elapsed_us_posix() / 1000ULL);
#endif
}

uint64_t compat_ticks_us(void)
{
#ifdef _WIN32
    return host_elapsed_us();
#else
    return host_elapsed_us_posix();
#endif
}

void compat_delay_ms(uint32_t milliseconds)
{
#ifdef _WIN32
    Sleep(milliseconds);
#else
    struct timespec req;
    req.tv_sec = (time_t)( milliseconds / 1000u);
    req.tv_nsec = (long)(milliseconds % 1000u) * 1000000L;
    nanosleep(&req, NULL);
#endif
}

void *compat_malloc(size_t size)
{
    return malloc(size);
}

void *compat_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

void *compat_large_malloc(size_t size)
{
    return malloc(size);
}

void *compat_large_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

void compat_free(void *ptr)
{
    free(ptr);
}
