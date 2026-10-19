/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "video_adapter.h"

#include <string.h>

#define RGB565_BLACK 0x0000u
#define RGB565_WHITE 0xffffu
#define RGB565_RED   0xf800u
#define RGB565_GREEN 0x07e0u

static uint16_t overlay_colour(unsigned native_x, unsigned native_y,
                               space_invaders_video_mode_t mode)
{
    unsigned raw_x;
    unsigned raw_y;
    if (mode == SPACE_INVADERS_VIDEO_MONO)
        return RGB565_WHITE;

    raw_x = 255u - native_y;
    raw_y = native_x;
    if (raw_x < 16u)
        return (raw_y < 16u || raw_y > 134u) ?
               RGB565_WHITE : RGB565_GREEN;
    if (raw_x <= 72u)
        return RGB565_GREEN;
    if (raw_x >= 192u && raw_x < 224u)
        return RGB565_RED;
    return RGB565_WHITE;
}

bool space_invaders_video_compose(
    const uint8_t *vram, size_t vram_size,
    uint16_t *frame, unsigned stride,
    space_invaders_video_mode_t mode)
{
    if (vram == NULL || vram_size < SPACE_INVADERS_VRAM_BYTES ||
        frame == NULL || stride < SPACE_INVADERS_FRAME_WIDTH ||
        (mode != SPACE_INVADERS_VIDEO_MONO &&
         mode != SPACE_INVADERS_VIDEO_CLASSIC_OVERLAY))
        return false;

    for (unsigned y = 0; y < SPACE_INVADERS_TARGET_HEIGHT; ++y) {
        unsigned source_y = y + y / 15u;
        bool second_y = y % 15u == 14u;
        unsigned raw_x = 255u - source_y;
        unsigned byte0 = raw_x >> 3;
        uint8_t mask0 = (uint8_t)(1u << (raw_x & 7u));
        unsigned byte1 = (raw_x - 1u) >> 3;
        uint8_t mask1 = (uint8_t)(1u << ((raw_x - 1u) & 7u));
        uint16_t *row = &frame[y * stride];
        unsigned source_x = 0u;
        unsigned group_x = 0u;

        memset(row, 0, SPACE_INVADERS_FRAME_X * sizeof(*row));
        memset(&row[SPACE_INVADERS_FRAME_X + SPACE_INVADERS_TARGET_WIDTH],
               0, SPACE_INVADERS_FRAME_X * sizeof(*row));
        row += SPACE_INVADERS_FRAME_X;
        for (unsigned x = 0; x < SPACE_INVADERS_TARGET_WIDTH; ++x) {
            const uint8_t *column = &vram[source_x * 32u];
            bool second_x = group_x == 14u;
            uint16_t colour = RGB565_BLACK;

            /* A 16:15 cell owns one or two source rows and columns. Keep
             * the first lit source pixel for the classic overlay colour. */
            if ((column[byte0] & mask0) != 0u)
                colour = overlay_colour(source_x, source_y, mode);
            else if (second_x && (column[32u + byte0] & mask0) != 0u)
                colour = overlay_colour(source_x + 1u, source_y, mode);
            else if (second_y && (column[byte1] & mask1) != 0u)
                colour = overlay_colour(source_x, source_y + 1u, mode);
            else if (second_x && second_y &&
                     (column[32u + byte1] & mask1) != 0u)
                colour = overlay_colour(source_x + 1u, source_y + 1u, mode);
            row[x] = colour;

            ++source_x;
            if (second_x) {
                ++source_x;
                group_x = 0u;
            } else {
                ++group_x;
            }
        }
    }
    return true;
}
