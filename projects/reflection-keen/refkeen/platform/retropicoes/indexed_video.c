#include "indexed_video.h"

#include <string.h>

void refkeen_indexed_row_bases(uint32_t out[200], uint32_t start,
                               uint32_t panning, uint32_t stride,
                               int split_line, uint32_t wrap_size)
{
    const uint32_t mask = wrap_size - 1u;
    uint32_t offset = (start + panning) & mask;
    for (int y = 0; y < 200; ++y) {
        out[y] = offset;
        offset = split_line == y ? panning & mask : (offset + stride) & mask;
    }
}

void refkeen_indexed_blit(const uint8_t *source, uint32_t wrap_size,
                          const uint32_t row_bases[200],
                          const uint16_t palette[256], uint16_t *dest,
                          int dest_stride, int dest_width, int dest_height,
                          int dx, int dy, int dw, int dh)
{
    uint16_t source_x[320];
    uint8_t source_row[320];
    int cached_y = -1;
    if (!source || !palette || !dest || dw <= 0 || dw > 320 || dh <= 0 ||
        dest_stride <= 0 || dest_width <= 0 || dest_height <= 0)
        return;
    for (int x = 0; x < dw; ++x)
        source_x[x] = (uint16_t)(x * 320 / dw);
    for (int y = 0; y < dh; ++y) {
        const int yy = dy + y;
        if ((unsigned)yy >= (unsigned)dest_height)
            continue;
        const int sy = y * 200 / dh;
        if (sy != cached_y) {
            const uint32_t offset = row_bases[sy];
            const uint32_t first = wrap_size - offset;
            if (first >= 320u)
                memcpy(source_row, source + offset, 320u);
            else {
                memcpy(source_row, source + offset, first);
                memcpy(source_row + first, source, 320u - first);
            }
            cached_y = sy;
        }
        for (int x = 0; x < dw; ++x) {
            const int xx = dx + x;
            if ((unsigned)xx < (unsigned)dest_width)
                dest[(size_t)yy * dest_stride + xx] =
                    palette[source_row[source_x[x]]];
        }
    }
}
