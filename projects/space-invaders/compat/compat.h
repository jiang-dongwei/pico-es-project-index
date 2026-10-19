/*
 * SDLPoP Pico compat layer, version 1.
 *
 * This is the platform boundary for the RP2350 port.  The game core must not
 * include pico_shared or FatFS directly; the target implementation owns the
 * hardware details behind these functions.
 */
#ifndef SDLPoP_COMPAT_H
#define SDLPoP_COMPAT_H

#include <stddef.h>
#include <stdint.h>

#include "video.h"

#define COMPAT_SCREEN_WIDTH  320u
#define COMPAT_SCREEN_HEIGHT 240u

/* Pico SDK link scripts copy .time_critical.* sections into internal SRAM. */
#if defined(SDLPoP_PLATFORM_PICO) && defined(__GNUC__)
#define COMPAT_HOT(name) __attribute__((section(".time_critical." #name)))
#else
#define COMPAT_HOT(name)
#endif

typedef uint16_t compat_pixel_t;

typedef enum {
    COMPAT_KEY_LEFT = 0,
    COMPAT_KEY_RIGHT,
    COMPAT_KEY_UP,
    COMPAT_KEY_DOWN,
    COMPAT_KEY_JUMP,
    COMPAT_KEY_ACTION,
    COMPAT_KEY_START,
    COMPAT_KEY_BACK,
    COMPAT_KEY_COUNT
} compat_key_t;

typedef struct {
    compat_pixel_t *pixels;
    unsigned stride;
    unsigned width;
    unsigned height;
} compat_framebuffer_t;

typedef void (*compat_audio_callback_t)(int16_t *samples, size_t sample_count,
                                        void *user_data);

/* Opaque handle owned by the FatFS-backed target implementation. */
typedef struct compat_file compat_file_t;
typedef struct compat_dir compat_dir_t;

typedef enum {
    COMPAT_PATH_MISSING = 0,
    COMPAT_PATH_FILE,
    COMPAT_PATH_DIRECTORY
} compat_path_type_t;

/* Called once before the game core starts. */
int compat_init(void);
void compat_shutdown(void);

/* Framebuffer is RGB565 and remains valid until compat_shutdown(). */
compat_framebuffer_t *compat_framebuffer(void);
void compat_present(void);

/* Input is sampled by the platform once per emulation frame. */
void compat_poll_input(void);
int compat_key_down(compat_key_t key);
int compat_key_pressed(compat_key_t key);

/* Audio callback is consumed by the platform's core-1 audio path. */
int compat_audio_start(unsigned sample_rate, unsigned channels,
                      compat_audio_callback_t callback, void *user_data);
void compat_audio_stop(void);

/* POSIX-like file boundary; the implementation maps paths to FatFS. */
compat_file_t *compat_file_open(const char *path, const char *mode);
size_t compat_file_read(void *ptr, size_t size, size_t count,
                        compat_file_t *file);
size_t compat_file_write(const void *ptr, size_t size, size_t count,
                         compat_file_t *file);
int compat_file_seek(compat_file_t *file, long offset, int origin);
long compat_file_tell(compat_file_t *file);
long compat_file_size(compat_file_t *file);
int compat_file_close(compat_file_t *file);
int compat_file_getc(compat_file_t *file);
int compat_file_putc(int value, compat_file_t *file);
int compat_file_puts(const char *text, compat_file_t *file);
char *compat_file_gets(char *text, int capacity, compat_file_t *file);
void compat_file_rewind(compat_file_t *file);

/* Path helpers return non-zero on success/existence. */
int compat_path_exists(const char *path);
compat_path_type_t compat_path_type(const char *path);
int compat_path_remove(const char *path);
int compat_dir_create(const char *path);
compat_dir_t *compat_dir_open(const char *path);
const char *compat_dir_read(compat_dir_t *directory);
void compat_dir_close(compat_dir_t *directory);

/* Monotonic time in milliseconds and microseconds. */
uint32_t compat_ticks_ms(void);
uint64_t compat_ticks_us(void);
void compat_delay_ms(uint32_t milliseconds);

/* Large allocations may be placed in PSRAM by the target implementation. */
void *compat_malloc(size_t size);
void *compat_calloc(size_t count, size_t size);
/* Explicit cold/large storage: Pico targets place this in PSRAM even when an
   individual allocation is smaller than the automatic routing threshold. */
void *compat_large_malloc(size_t size);
void *compat_large_calloc(size_t count, size_t size);
void compat_free(void *ptr);

#endif
