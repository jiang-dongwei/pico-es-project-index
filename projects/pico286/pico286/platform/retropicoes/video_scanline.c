#include "video_scanline.h"

static uint16_t required_palette_entries(pico286_scanline_format_t format)
{
    switch (format) {
        case PICO286_SCANLINE_INDEX8:
            return 256u;
        case PICO286_SCANLINE_PACKED_4BPP:
            return 16u;
        case PICO286_SCANLINE_PACKED_2BPP:
            return 4u;
        case PICO286_SCANLINE_PACKED_1BPP:
            return 2u;
        default:
            return 0u;
    }
}

static uint8_t decode_index(const pico286_scanline_source_t *source,
                            uint16_t x)
{
    switch (source->format) {
        case PICO286_SCANLINE_INDEX8:
            return source->pixels[x];
        case PICO286_SCANLINE_PACKED_4BPP: {
            const uint8_t packed = source->pixels[x >> 1u];
            return (uint8_t)((x & 1u) == 0u ? packed >> 4u
                                            : packed & 0x0fu);
        }
        case PICO286_SCANLINE_PACKED_2BPP: {
            const uint8_t packed = source->pixels[x >> 2u];
            const uint8_t shift = (uint8_t)((3u - (x & 3u)) * 2u);
            return (uint8_t)((packed >> shift) & 3u);
        }
        case PICO286_SCANLINE_PACKED_1BPP: {
            const uint8_t packed = source->pixels[x >> 3u];
            const uint8_t shift = (uint8_t)(7u - (x & 7u));
            return (uint8_t)((packed >> shift) & 1u);
        }
        default:
            return 0u;
    }
}

pico286_scanline_status_t pico286_convert_scanline_rgb565(
    const pico286_scanline_source_t *source,
    uint16_t *target,
    uint16_t target_width)
{
    uint16_t palette_required;
    uint32_t bits_per_pixel;
    size_t bytes_required;

    if ((source == NULL) || (target == NULL) ||
        (source->pixels == NULL) || (source->palette == NULL)) {
        return PICO286_SCANLINE_NULL_ARGUMENT;
    }
    if ((source->width == 0u) || (target_width == 0u)) {
        return PICO286_SCANLINE_EMPTY;
    }

    palette_required = required_palette_entries(source->format);
    if (palette_required == 0u) {
        return PICO286_SCANLINE_BAD_FORMAT;
    }
    if (source->palette_entries < palette_required) {
        return PICO286_SCANLINE_PALETTE_SHORT;
    }

    bits_per_pixel = source->format == PICO286_SCANLINE_INDEX8
                         ? 8u
                         : source->format == PICO286_SCANLINE_PACKED_4BPP
                               ? 4u
                               : source->format == PICO286_SCANLINE_PACKED_2BPP
                                     ? 2u
                                     : 1u;
    bytes_required = ((size_t)source->width * bits_per_pixel + 7u) / 8u;
    if (source->byte_count < bytes_required) {
        return PICO286_SCANLINE_SOURCE_SHORT;
    }

    for (uint16_t x = 0u; x < target_width; ++x) {
        const uint16_t source_x = (uint16_t)(
            ((uint32_t)x * source->width) / target_width);
        target[x] = source->palette[decode_index(source, source_x)];
    }
    return PICO286_SCANLINE_OK;
}

pico286_scanline_status_t pico286_render_text_scanline_rgb565(
    const pico286_text_scanline_source_t *source,
    uint16_t *target,
    uint16_t target_width)
{
    enum {
        TEXT_COLUMNS = 80u,
        TEXT_ROWS = 25u,
        GLYPH_WIDTH = 8u,
        GLYPH_HEIGHT = 16u,
        TEXT_SOURCE_WIDTH = TEXT_COLUMNS * GLYPH_WIDTH,
        TEXT_SOURCE_HEIGHT = TEXT_ROWS * GLYPH_HEIGHT,
        TEXT_CELL_BYTES = TEXT_COLUMNS * TEXT_ROWS * 2u,
        FONT_BYTES = 256u * GLYPH_HEIGHT
    };

    if ((source == NULL) || (target == NULL) || (source->cells == NULL) ||
        (source->font_8x16 == NULL) || (source->palette == NULL)) {
        return PICO286_SCANLINE_NULL_ARGUMENT;
    }
    if (target_width == 0u) {
        return PICO286_SCANLINE_EMPTY;
    }
    if ((source->cell_byte_count < TEXT_CELL_BYTES) ||
        (source->font_byte_count < FONT_BYTES) ||
        (source->source_line >= TEXT_SOURCE_HEIGHT)) {
        return PICO286_SCANLINE_SOURCE_SHORT;
    }
    if (source->palette_entries < 16u) {
        return PICO286_SCANLINE_PALETTE_SHORT;
    }

    for (uint16_t x = 0u; x < target_width; ++x) {
        const uint16_t source_x = (uint16_t)(
            ((uint32_t)x * TEXT_SOURCE_WIDTH) / target_width);
        const uint16_t column = source_x / GLYPH_WIDTH;
        const uint16_t row = source->source_line / GLYPH_HEIGHT;
        const uint8_t glyph_row = (uint8_t)(
            source->source_line % GLYPH_HEIGHT);
        const size_t cell_offset =
            ((size_t)row * TEXT_COLUMNS + column) * 2u;
        const uint8_t character = source->cells[cell_offset];
        const uint8_t attribute = source->cells[cell_offset + 1u];
        const uint8_t glyph = source->font_8x16[
            (size_t)character * GLYPH_HEIGHT + glyph_row];
        const uint8_t bit = (uint8_t)(7u - (source_x % GLYPH_WIDTH));
        const uint8_t foreground = attribute & 0x0fu;
        const uint8_t background = (attribute >> 4u) & 0x07u;
        const uint8_t visible = (uint8_t)(
            ((glyph >> bit) & 1u) != 0u &&
            (((attribute & 0x80u) == 0u) || source->blink_visible != 0u));
        target[x] = source->palette[visible != 0u ? foreground : background];
    }
    return PICO286_SCANLINE_OK;
}
