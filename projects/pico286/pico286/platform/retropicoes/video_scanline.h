#ifndef PICO286_VIDEO_SCANLINE_H
#define PICO286_VIDEO_SCANLINE_H

#include <stddef.h>
#include <stdint.h>

typedef enum pico286_scanline_format {
    PICO286_SCANLINE_INDEX8 = 0,
    PICO286_SCANLINE_PACKED_4BPP,
    PICO286_SCANLINE_PACKED_2BPP,
    PICO286_SCANLINE_PACKED_1BPP
} pico286_scanline_format_t;

typedef enum pico286_scanline_status {
    PICO286_SCANLINE_OK = 0,
    PICO286_SCANLINE_NULL_ARGUMENT,
    PICO286_SCANLINE_EMPTY,
    PICO286_SCANLINE_BAD_FORMAT,
    PICO286_SCANLINE_SOURCE_SHORT,
    PICO286_SCANLINE_PALETTE_SHORT
} pico286_scanline_status_t;

typedef struct pico286_scanline_source {
    const uint8_t *pixels;
    size_t byte_count;
    uint16_t width;
    pico286_scanline_format_t format;
    const uint16_t *palette;
    uint16_t palette_entries;
} pico286_scanline_source_t;

typedef struct pico286_text_scanline_source {
    const uint8_t *cells;
    size_t cell_byte_count;
    const uint8_t *font_8x16;
    size_t font_byte_count;
    const uint16_t *palette;
    uint16_t palette_entries;
    uint16_t source_line;
    uint8_t blink_visible;
} pico286_text_scanline_source_t;

pico286_scanline_status_t pico286_convert_scanline_rgb565(
    const pico286_scanline_source_t *source,
    uint16_t *target,
    uint16_t target_width);
pico286_scanline_status_t pico286_render_text_scanline_rgb565(
    const pico286_text_scanline_source_t *source,
    uint16_t *target,
    uint16_t target_width);

#endif
