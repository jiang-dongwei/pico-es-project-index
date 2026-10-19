/* Platform-neutral video types for the SDLPoP Pico port. */
#ifndef SDLPOP_COMPAT_VIDEO_H
#define SDLPOP_COMPAT_VIDEO_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} compat_color_t;

typedef struct {
    compat_color_t colors[256];
    uint16_t count;
} compat_palette_t;

typedef enum {
    COMPAT_PIXEL_INDEX8 = 1,
    COMPAT_PIXEL_RGB565,
    COMPAT_PIXEL_RGB888,
    COMPAT_PIXEL_RGBA8888
} compat_pixel_format_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} compat_rect_t;

/*
 * Pixels are owned by the caller or backend. INDEX8 surfaces require a
 * palette; direct-color surfaces leave palette as NULL.
 */
typedef struct compat_surface {
    void *pixels;
    size_t pitch;
    uint16_t w;
    uint16_t h;
    compat_pixel_format_t format;
    compat_palette_t *palette;
    compat_rect_t clip;
    uint8_t owns_pixels;
    uint8_t owns_palette;
} compat_surface_t;

size_t compat_video_bytes_per_pixel(compat_pixel_format_t format);
int compat_surface_init(compat_surface_t *surface, void *pixels,
                        uint16_t width, uint16_t height, size_t stride_bytes,
                        compat_pixel_format_t format,
                        compat_palette_t *palette);
int compat_surface_set_clip(compat_surface_t *surface,
                            const compat_rect_t *rect);
void *compat_surface_row(compat_surface_t *surface, uint16_t y);
const void *compat_surface_const_row(const compat_surface_t *surface,
                                     uint16_t y);
uint16_t compat_video_rgb565(uint8_t r, uint8_t g, uint8_t b);
int compat_surface_blit_rgb565(const compat_surface_t *source,
                               compat_surface_t *target,
                               int target_x, int target_y);
int compat_surface_blit_rgb565_rect(const compat_surface_t *source,
                                    const compat_rect_t *source_rect,
                                    compat_surface_t *target,
                                    const compat_rect_t *target_rect,
                                    int transparent_index_zero);
int compat_surface_blit_mono_rgb565(const compat_surface_t *source,
                                    compat_surface_t *target,
                                    int target_x, int target_y,
                                    uint16_t color);
int compat_surface_blit_xor_rgb565(const compat_surface_t *source,
                                   compat_surface_t *target,
                                   int target_x, int target_y);
int compat_surface_fill_rgb565(compat_surface_t *surface,
                               const compat_rect_t *rect, uint16_t color);
int compat_surface_fill_rgba_rgb565(compat_surface_t *surface,
                                    const compat_rect_t *rect,
                                    compat_color_t color);
int compat_surface_scale_rgb565(const compat_surface_t *source,
                                compat_surface_t *target,
                                uint8_t brightness);
int compat_surface_flip_horizontal(const compat_surface_t *source,
                                   compat_surface_t *target);
int compat_surface_flip_vertical(compat_surface_t *surface,
                                 void *row_scratch, size_t scratch_size);

#endif
