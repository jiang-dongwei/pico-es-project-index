#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "indexed_video.h"

enum { WIDTH = 320, HEIGHT = 240, PIXELS = WIDTH * HEIGHT };

static int run_case(uint32_t wrap_size, uint32_t start, uint32_t panning,
                    uint32_t stride, int split, int dx, int dy, int dw, int dh)
{
    uint8_t *source = malloc(wrap_size);
    uint16_t *actual = malloc(PIXELS * sizeof(*actual));
    uint16_t *expected = malloc(PIXELS * sizeof(*expected));
    uint16_t palette[256];
    uint32_t rows[200];
    int failures = 0;
    if (!source || !actual || !expected) {
        free(source); free(actual); free(expected);
        return 1;
    }
    for (uint32_t i = 0; i < wrap_size; ++i)
        source[i] = (uint8_t)((i * 37u + i / 257u) & 255u);
    for (unsigned i = 0; i < 256u; ++i)
        palette[i] = (uint16_t)((i * 977u) ^ (i << 8));
    for (unsigned i = 0; i < PIXELS; ++i)
        actual[i] = expected[i] = 0x1234u;
    refkeen_indexed_row_bases(rows, start, panning, stride, split, wrap_size);
    refkeen_indexed_blit(source, wrap_size, rows, palette, actual,
                         WIDTH, WIDTH, HEIGHT, dx, dy, dw, dh);

    uint32_t offset = (start + panning) & (wrap_size - 1u);
    for (int sy = 0; sy < 200; ++sy) {
        if (rows[sy] != offset) ++failures;
        offset = split == sy ? panning & (wrap_size - 1u) :
                 (offset + stride) & (wrap_size - 1u);
    }
    for (int y = 0; y < dh; ++y) {
        const int yy = dy + y;
        if ((unsigned)yy >= HEIGHT) continue;
        const uint32_t row = rows[y * 200 / dh];
        for (int x = 0; x < dw; ++x) {
            const int xx = dx + x;
            if ((unsigned)xx >= WIDTH) continue;
            const uint32_t sx = (uint32_t)(x * 320 / dw);
            expected[yy * WIDTH + xx] =
                palette[source[(row + sx) & (wrap_size - 1u)]];
        }
    }
    if (memcmp(actual, expected, PIXELS * sizeof(*actual)) != 0)
        ++failures;
    free(source); free(actual); free(expected);
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += run_case(0x80000u, 123u, 2u, 320u, -1, 7, 4, 304, 231);
    failures += run_case(0x80000u, 0xfffeu, 1u, 319u, 100, 0, 0, 320, 240);
    failures += run_case(0x40000u, 0x3ff80u, 3u, 320u, 0, 3, 4, 312, 231);
    printf("REFKEEN INDEXED VIDEO: %s (3 cases)\n",
           failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
