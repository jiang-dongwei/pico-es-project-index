#include "video.h"
#include "compat.h"

size_t compat_video_bytes_per_pixel(compat_pixel_format_t format)
{
    switch (format) {
        case COMPAT_PIXEL_INDEX8: return 1u;
        case COMPAT_PIXEL_RGB565: return 2u;
        case COMPAT_PIXEL_RGB888: return 3u;
        case COMPAT_PIXEL_RGBA8888: return 4u;
        default: return 0u;
    }
}

int compat_surface_init(compat_surface_t *surface, void *pixels,
                        uint16_t width, uint16_t height, size_t stride_bytes,
                        compat_pixel_format_t format,
                        compat_palette_t *palette)
{
    const size_t bytes_per_pixel = compat_video_bytes_per_pixel(format);

    if (surface == NULL || pixels == NULL || width == 0u || height == 0u ||
        bytes_per_pixel == 0u ||
        stride_bytes < (size_t)width * bytes_per_pixel ||
        (format == COMPAT_PIXEL_INDEX8 && palette == NULL)) {
        return -1;
    }

    surface->pixels = pixels;
    surface->pitch = stride_bytes;
    surface->w = width;
    surface->h = height;
    surface->format = format;
    surface->palette = palette;
    surface->clip.x = 0;
    surface->clip.y = 0;
    surface->clip.w = width;
    surface->clip.h = height;
    surface->owns_pixels = 0u;
    surface->owns_palette = 0u;
    return 0;
}

int compat_surface_set_clip(compat_surface_t *surface,
                            const compat_rect_t *rect)
{
    int x0;
    int y0;
    int x1;
    int y1;

    if (surface == NULL) return -1;
    if (rect == NULL) {
        surface->clip.x = 0;
        surface->clip.y = 0;
        surface->clip.w = surface->w;
        surface->clip.h = surface->h;
        return 0;
    }

    x0 = rect->x;
    y0 = rect->y;
    x1 = rect->x + rect->w;
    y1 = rect->y + rect->h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > surface->w) x1 = surface->w;
    if (y1 > surface->h) y1 = surface->h;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    surface->clip.x = x0;
    surface->clip.y = y0;
    surface->clip.w = x1 - x0;
    surface->clip.h = y1 - y0;
    return 0;
}

void *compat_surface_row(compat_surface_t *surface, uint16_t y)
{
    if (surface == NULL || surface->pixels == NULL || y >= surface->h) {
        return NULL;
    }
    return (uint8_t *)surface->pixels + (size_t)y * surface->pitch;
}

const void *compat_surface_const_row(const compat_surface_t *surface,
                                     uint16_t y)
{
    if (surface == NULL || surface->pixels == NULL || y >= surface->h) {
        return NULL;
    }
    return (const uint8_t *)surface->pixels +
           (size_t)y * surface->pitch;
}

uint16_t compat_video_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)((((uint16_t)r & 0xf8u) << 8) |
                      (((uint16_t)g & 0xfcu) << 3) |
                      ((uint16_t)b >> 3));
}

static uint16_t blend_rgb565(uint16_t destination, compat_color_t source)
{
    uint32_t alpha = source.a;
    uint32_t inverse_alpha = 255u - alpha;
    uint32_t dst_r = ((destination >> 11) & 0x1fu) * 255u / 31u;
    uint32_t dst_g = ((destination >> 5) & 0x3fu) * 255u / 63u;
    uint32_t dst_b = (destination & 0x1fu) * 255u / 31u;
    uint8_t out_r;
    uint8_t out_g;
    uint8_t out_b;

    if (alpha == 255u) {
        return compat_video_rgb565(source.r, source.g, source.b);
    }
    if (alpha == 0u) {
        return destination;
    }

    out_r = (uint8_t)((source.r * alpha + dst_r * inverse_alpha + 127u) /
                      255u);
    out_g = (uint8_t)((source.g * alpha + dst_g * inverse_alpha + 127u) /
                      255u);
    out_b = (uint8_t)((source.b * alpha + dst_b * inverse_alpha + 127u) /
                      255u);
    return compat_video_rgb565(out_r, out_g, out_b);
}

static compat_color_t read_source_color(const compat_surface_t *source,
                                        const uint8_t *pixel)
{
    compat_color_t color = {0u, 0u, 0u, 255u};

    switch (source->format) {
        case COMPAT_PIXEL_INDEX8:
            color = source->palette->colors[*pixel];
            break;
        case COMPAT_PIXEL_RGB565: {
            uint16_t packed = (uint16_t)(pixel[0] | ((uint16_t)pixel[1] << 8));
            color.r = (uint8_t)(((packed >> 11) & 0x1fu) * 255u / 31u);
            color.g = (uint8_t)(((packed >> 5) & 0x3fu) * 255u / 63u);
            color.b = (uint8_t)((packed & 0x1fu) * 255u / 31u);
            break;
        }
        case COMPAT_PIXEL_RGB888:
            color.r = pixel[0];
            color.g = pixel[1];
            color.b = pixel[2];
            break;
        case COMPAT_PIXEL_RGBA8888:
            color.r = pixel[0];
            color.g = pixel[1];
            color.b = pixel[2];
            color.a = pixel[3];
            break;
        default:
            color.a = 0u;
            break;
    }
    return color;
}

int compat_surface_blit_rgb565(const compat_surface_t *source,
                               compat_surface_t *target,
                               int target_x, int target_y)
{
    compat_rect_t target_rect;

    if (source == NULL) {
        return -1;
    }
    target_rect.x = target_x;
    target_rect.y = target_y;
    target_rect.w = source->w;
    target_rect.h = source->h;
    return compat_surface_blit_rgb565_rect(source, NULL, target, &target_rect,
                                           0);
}

COMPAT_HOT(compat_surface_blit_rgb565_rect)
int compat_surface_blit_rgb565_rect(const compat_surface_t *source,
                                    const compat_rect_t *source_rect,
                                    compat_surface_t *target,
                                    const compat_rect_t *target_rect,
                                    int transparent_index_zero)
{
    int source_x = 0;
    int source_y = 0;
    int target_x = 0;
    int target_y = 0;
    int copy_width;
    int copy_height;
    size_t source_bpp;
    int y;

    if (source == NULL || target == NULL || source->pixels == NULL ||
        target->pixels == NULL || target->format != COMPAT_PIXEL_RGB565 ||
        (source->format == COMPAT_PIXEL_INDEX8 && source->palette == NULL)) {
        return -1;
    }

    source_bpp = compat_video_bytes_per_pixel(source->format);
    if (source_bpp == 0u) {
        return -1;
    }

    copy_width = source->w;
    copy_height = source->h;
    if (source_rect != NULL) {
        source_x = source_rect->x;
        source_y = source_rect->y;
        copy_width = source_rect->w;
        copy_height = source_rect->h;
    }
    if (target_rect != NULL) {
        target_x = target_rect->x;
        target_y = target_rect->y;
        if (target_rect->w < copy_width) copy_width = target_rect->w;
        if (target_rect->h < copy_height) copy_height = target_rect->h;
    } else {
        target_x = 0;
        target_y = 0;
    }
    if (source_x < 0) {
        target_x -= source_x;
        copy_width += source_x;
        source_x = 0;
    }
    if (source_y < 0) {
        target_y -= source_y;
        copy_height += source_y;
        source_y = 0;
    }
    if (source_x + copy_width > source->w) {
        copy_width = source->w - source_x;
    }
    if (source_y + copy_height > source->h) {
        copy_height = source->h - source_y;
    }
    if (target_x < 0) {
        int clipped = -target_x;
        source_x += clipped;
        copy_width -= clipped;
        target_x = 0;
    }
    if (target_y < 0) {
        int clipped = -target_y;
        source_y += clipped;
        copy_height -= clipped;
        target_y = 0;
    }
    if (target_x + copy_width > target->w) {
        copy_width = target->w - target_x;
    }
    if (target_y + copy_height > target->h) {
        copy_height = target->h - target_y;
    }
    if (target_x < target->clip.x) {
        int clipped = target->clip.x - target_x;
        source_x += clipped;
        copy_width -= clipped;
        target_x = target->clip.x;
    }
    if (target_y < target->clip.y) {
        int clipped = target->clip.y - target_y;
        source_y += clipped;
        copy_height -= clipped;
        target_y = target->clip.y;
    }
    if (target_x + copy_width > target->clip.x + target->clip.w) {
        copy_width = target->clip.x + target->clip.w - target_x;
    }
    if (target_y + copy_height > target->clip.y + target->clip.h) {
        copy_height = target->clip.y + target->clip.h - target_y;
    }
    if (copy_width <= 0 || copy_height <= 0) {
        return 0;
    }

    for (y = 0; y < copy_height; ++y) {
        const uint8_t *source_pixel =
            (const uint8_t *)compat_surface_const_row(
                source, (uint16_t)(source_y + y)) +
            (size_t)source_x * source_bpp;
        uint8_t *target_pixel =
            (uint8_t *)compat_surface_row(target,
                                          (uint16_t)(target_y + y)) +
            (size_t)target_x * 2u;
        int x;

        for (x = 0; x < copy_width; ++x) {
            uint16_t destination =
                (uint16_t)(target_pixel[0] | ((uint16_t)target_pixel[1] << 8));
            compat_color_t source_color = read_source_color(source, source_pixel);
            uint16_t result;

            if (source->format == COMPAT_PIXEL_INDEX8) {
                if (transparent_index_zero && *source_pixel == 0u) {
                    source_pixel += source_bpp;
                    target_pixel += 2;
                    continue;
                }
                if (!transparent_index_zero) source_color.a = 255u;
            } else if (source->format == COMPAT_PIXEL_RGB565 &&
                       transparent_index_zero && source_pixel[0] == 0u &&
                       source_pixel[1] == 0u) {
                source_pixel += source_bpp;
                target_pixel += 2;
                continue;
            }
            result = blend_rgb565(destination, source_color);
            target_pixel[0] = (uint8_t)(result & 0xffu);
            target_pixel[1] = (uint8_t)(result >> 8);
            source_pixel += source_bpp;
            target_pixel += 2;
        }
    }
    return 0;
}

static int clip_whole_blit(const compat_surface_t *source,
                           const compat_surface_t *target,
                           int *source_x, int *source_y,
                           int *target_x, int *target_y,
                           int *copy_width, int *copy_height)
{
    *source_x = 0;
    *source_y = 0;
    *copy_width = source->w;
    *copy_height = source->h;
    if (*target_x < target->clip.x) {
        int clipped = target->clip.x - *target_x;
        *source_x += clipped;
        *copy_width -= clipped;
        *target_x = target->clip.x;
    }
    if (*target_y < target->clip.y) {
        int clipped = target->clip.y - *target_y;
        *source_y += clipped;
        *copy_height -= clipped;
        *target_y = target->clip.y;
    }
    if (*target_x + *copy_width > target->clip.x + target->clip.w) {
        *copy_width = target->clip.x + target->clip.w - *target_x;
    }
    if (*target_y + *copy_height > target->clip.y + target->clip.h) {
        *copy_height = target->clip.y + target->clip.h - *target_y;
    }
    return (*copy_width > 0 && *copy_height > 0) ? 1 : 0;
}

COMPAT_HOT(compat_surface_blit_mono_rgb565)
int compat_surface_blit_mono_rgb565(const compat_surface_t *source,
                                    compat_surface_t *target,
                                    int target_x, int target_y,
                                    uint16_t color)
{
    int source_x;
    int source_y;
    int copy_width;
    int copy_height;
    int y;

    if (source == NULL || target == NULL || source->pixels == NULL ||
        target->pixels == NULL || source->format != COMPAT_PIXEL_INDEX8 ||
        target->format != COMPAT_PIXEL_RGB565) {
        return -1;
    }
    if (!clip_whole_blit(source, target, &source_x, &source_y,
                         &target_x, &target_y, &copy_width, &copy_height)) {
        return 0;
    }
    for (y = 0; y < copy_height; ++y) {
        const uint8_t *source_pixel =
            (const uint8_t *)compat_surface_const_row(
                source, (uint16_t)(source_y + y)) + source_x;
        uint8_t *target_pixel =
            (uint8_t *)compat_surface_row(target,
                                          (uint16_t)(target_y + y)) +
            (size_t)target_x * 2u;
        int x;

        for (x = 0; x < copy_width; ++x) {
            if (*source_pixel != 0u) {
                target_pixel[0] = (uint8_t)(color & 0xffu);
                target_pixel[1] = (uint8_t)(color >> 8);
            }
            ++source_pixel;
            target_pixel += 2;
        }
    }
    return 0;
}

COMPAT_HOT(compat_surface_blit_xor_rgb565)
int compat_surface_blit_xor_rgb565(const compat_surface_t *source,
                                   compat_surface_t *target,
                                   int target_x, int target_y)
{
    int source_x;
    int source_y;
    int copy_width;
    int copy_height;
    size_t source_bpp;
    int y;

    if (source == NULL || target == NULL || source->pixels == NULL ||
        target->pixels == NULL || target->format != COMPAT_PIXEL_RGB565 ||
        (source->format == COMPAT_PIXEL_INDEX8 && source->palette == NULL)) {
        return -1;
    }
    source_bpp = compat_video_bytes_per_pixel(source->format);
    if (source_bpp == 0u) return -1;
    if (!clip_whole_blit(source, target, &source_x, &source_y,
                         &target_x, &target_y, &copy_width, &copy_height)) {
        return 0;
    }
    for (y = 0; y < copy_height; ++y) {
        const uint8_t *source_pixel =
            (const uint8_t *)compat_surface_const_row(
                source, (uint16_t)(source_y + y)) +
            (size_t)source_x * source_bpp;
        uint8_t *target_pixel =
            (uint8_t *)compat_surface_row(target,
                                          (uint16_t)(target_y + y)) +
            (size_t)target_x * 2u;
        int x;

        for (x = 0; x < copy_width; ++x) {
            compat_color_t source_color = read_source_color(source,
                                                             source_pixel);
            uint16_t destination =
                (uint16_t)(target_pixel[0] |
                           ((uint16_t)target_pixel[1] << 8));
            uint16_t packed = compat_video_rgb565(source_color.r,
                                                   source_color.g,
                                                   source_color.b);
            uint16_t result = destination ^ packed;
            target_pixel[0] = (uint8_t)(result & 0xffu);
            target_pixel[1] = (uint8_t)(result >> 8);
            source_pixel += source_bpp;
            target_pixel += 2;
        }
    }
    return 0;
}

COMPAT_HOT(compat_surface_fill_rgb565)
int compat_surface_fill_rgb565(compat_surface_t *surface,
                               const compat_rect_t *rect, uint16_t color)
{
    int x0 = 0;
    int y0 = 0;
    int width;
    int height;
    int y;

    if (surface == NULL || surface->pixels == NULL ||
        surface->format != COMPAT_PIXEL_RGB565) {
        return -1;
    }
    width = surface->w;
    height = surface->h;
    if (rect != NULL) {
        x0 = rect->x;
        y0 = rect->y;
        width = rect->w;
        height = rect->h;
    }
    if (x0 < 0) { width += x0; x0 = 0; }
    if (y0 < 0) { height += y0; y0 = 0; }
    if (x0 + width > surface->w) width = surface->w - x0;
    if (y0 + height > surface->h) height = surface->h - y0;
    if (x0 < surface->clip.x) {
        width -= surface->clip.x - x0;
        x0 = surface->clip.x;
    }
    if (y0 < surface->clip.y) {
        height -= surface->clip.y - y0;
        y0 = surface->clip.y;
    }
    if (x0 + width > surface->clip.x + surface->clip.w) {
        width = surface->clip.x + surface->clip.w - x0;
    }
    if (y0 + height > surface->clip.y + surface->clip.h) {
        height = surface->clip.y + surface->clip.h - y0;
    }
    if (width <= 0 || height <= 0) return 0;

    for (y = 0; y < height; ++y) {
        uint8_t *pixel = (uint8_t *)compat_surface_row(
            surface, (uint16_t)(y0 + y)) + (size_t)x0 * 2u;
        int x;
        for (x = 0; x < width; ++x) {
            pixel[0] = (uint8_t)(color & 0xffu);
            pixel[1] = (uint8_t)(color >> 8);
            pixel += 2;
        }
    }
    return 0;
}

COMPAT_HOT(compat_surface_fill_rgba_rgb565)
int compat_surface_fill_rgba_rgb565(compat_surface_t *surface,
                                    const compat_rect_t *rect,
                                    compat_color_t color)
{
    int x0 = 0;
    int y0 = 0;
    int width;
    int height;
    int y;

    if (surface == NULL || surface->pixels == NULL ||
        surface->format != COMPAT_PIXEL_RGB565) {
        return -1;
    }
    width = surface->w;
    height = surface->h;
    if (rect != NULL) {
        x0 = rect->x;
        y0 = rect->y;
        width = rect->w;
        height = rect->h;
    }
    if (x0 < surface->clip.x) {
        width -= surface->clip.x - x0;
        x0 = surface->clip.x;
    }
    if (y0 < surface->clip.y) {
        height -= surface->clip.y - y0;
        y0 = surface->clip.y;
    }
    if (x0 + width > surface->clip.x + surface->clip.w) {
        width = surface->clip.x + surface->clip.w - x0;
    }
    if (y0 + height > surface->clip.y + surface->clip.h) {
        height = surface->clip.y + surface->clip.h - y0;
    }
    if (width <= 0 || height <= 0) return 0;

    for (y = 0; y < height; ++y) {
        uint8_t *pixel = (uint8_t *)compat_surface_row(
            surface, (uint16_t)(y0 + y)) + (size_t)x0 * 2u;
        int x;
        for (x = 0; x < width; ++x) {
            uint16_t destination =
                (uint16_t)(pixel[0] | ((uint16_t)pixel[1] << 8));
            uint16_t result = blend_rgb565(destination, color);
            pixel[0] = (uint8_t)(result & 0xffu);
            pixel[1] = (uint8_t)(result >> 8);
            pixel += 2;
        }
    }
    return 0;
}

COMPAT_HOT(compat_surface_scale_rgb565)
int compat_surface_scale_rgb565(const compat_surface_t *source,
                                compat_surface_t *target,
                                uint8_t brightness)
{
    uint16_t width;
    uint16_t height;
    uint16_t y;

    if (source == NULL || target == NULL || source->pixels == NULL ||
        target->pixels == NULL || source->format != COMPAT_PIXEL_RGB565 ||
        target->format != COMPAT_PIXEL_RGB565) {
        return -1;
    }
    width = source->w < target->w ? source->w : target->w;
    height = source->h < target->h ? source->h : target->h;
    for (y = 0; y < height; ++y) {
        const uint8_t *source_pixel =
            (const uint8_t *)compat_surface_const_row(source, y);
        uint8_t *target_pixel = (uint8_t *)compat_surface_row(target, y);
        uint16_t x;

        for (x = 0; x < width; ++x) {
            uint16_t packed =
                (uint16_t)(source_pixel[0] |
                           ((uint16_t)source_pixel[1] << 8));
            uint8_t r = (uint8_t)(((packed >> 11) & 0x1fu) * 255u / 31u);
            uint8_t g = (uint8_t)(((packed >> 5) & 0x3fu) * 255u / 63u);
            uint8_t b = (uint8_t)((packed & 0x1fu) * 255u / 31u);
            uint16_t result;

            r = (uint8_t)(((uint16_t)r * brightness + 127u) / 255u);
            g = (uint8_t)(((uint16_t)g * brightness + 127u) / 255u);
            b = (uint8_t)(((uint16_t)b * brightness + 127u) / 255u);
            result = compat_video_rgb565(r, g, b);
            target_pixel[0] = (uint8_t)(result & 0xffu);
            target_pixel[1] = (uint8_t)(result >> 8);
            source_pixel += 2;
            target_pixel += 2;
        }
    }
    return 0;
}

int compat_surface_flip_horizontal(const compat_surface_t *source,
                                   compat_surface_t *target)
{
    size_t bytes_per_pixel;
    uint16_t y;

    if (source == NULL || target == NULL || source->pixels == NULL ||
        target->pixels == NULL || source->w != target->w ||
        source->h != target->h || source->format != target->format) {
        return -1;
    }
    bytes_per_pixel = compat_video_bytes_per_pixel(source->format);
    if (bytes_per_pixel == 0u) return -1;
    for (y = 0; y < source->h; ++y) {
        const uint8_t *source_row =
            (const uint8_t *)compat_surface_const_row(source, y);
        uint8_t *target_row = (uint8_t *)compat_surface_row(target, y);
        uint16_t x;

        for (x = 0; x < source->w; ++x) {
            const uint8_t *source_pixel =
                source_row + (size_t)x * bytes_per_pixel;
            uint8_t *target_pixel =
                target_row + (size_t)(source->w - 1u - x) * bytes_per_pixel;
            size_t byte_index;
            for (byte_index = 0; byte_index < bytes_per_pixel; ++byte_index) {
                target_pixel[byte_index] = source_pixel[byte_index];
            }
        }
    }
    return 0;
}

int compat_surface_flip_vertical(compat_surface_t *surface,
                                 void *row_scratch, size_t scratch_size)
{
    uint8_t *top;
    uint8_t *bottom;
    uint16_t y;

    if (surface == NULL || surface->pixels == NULL || row_scratch == NULL ||
        scratch_size < surface->pitch) {
        return -1;
    }
    top = (uint8_t *)surface->pixels;
    bottom = top + (size_t)(surface->h - 1u) * surface->pitch;
    for (y = 0; y < surface->h / 2u; ++y) {
        size_t byte_index;
        uint8_t *scratch = (uint8_t *)row_scratch;
        for (byte_index = 0; byte_index < surface->pitch; ++byte_index) {
            scratch[byte_index] = top[byte_index];
            top[byte_index] = bottom[byte_index];
            bottom[byte_index] = scratch[byte_index];
        }
        top += surface->pitch;
        bottom -= surface->pitch;
    }
    return 0;
}
