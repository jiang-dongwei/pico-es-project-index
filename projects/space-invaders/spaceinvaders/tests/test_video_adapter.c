/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "video_adapter.h"

#include <stdio.h>
#include <string.h>

static unsigned failures;
static uint8_t vram[SPACE_INVADERS_VRAM_BYTES];
static uint16_t frame[SPACE_INVADERS_FRAME_WIDTH *
                      SPACE_INVADERS_FRAME_HEIGHT];

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

static void set_raw_pixel(unsigned raw_x, unsigned raw_y)
{
    vram[raw_y * 32u + raw_x / 8u] |= (uint8_t)(1u << (raw_x & 7u));
}

int main(void)
{
    memset(vram, 0, sizeof(vram));
    memset(frame, 0xa5, sizeof(frame));
    set_raw_pixel(255u, 0u);   /* native 0,0: white */
    set_raw_pixel(20u, 10u);   /* native 10,235: green */
    set_raw_pixel(200u, 100u); /* native 100,55: red */
    set_raw_pixel(240u, 15u);  /* native 15,15: both axes skipped by sampling */

    CHECK(!space_invaders_video_compose(
        NULL, sizeof(vram), frame, SPACE_INVADERS_FRAME_WIDTH,
        SPACE_INVADERS_VIDEO_CLASSIC_OVERLAY));
    CHECK(!space_invaders_video_compose(
        vram, sizeof(vram) - 1u, frame, SPACE_INVADERS_FRAME_WIDTH,
        SPACE_INVADERS_VIDEO_CLASSIC_OVERLAY));
    CHECK(!space_invaders_video_compose(
        vram, sizeof(vram), frame, SPACE_INVADERS_FRAME_WIDTH - 1u,
        SPACE_INVADERS_VIDEO_CLASSIC_OVERLAY));
    CHECK(space_invaders_video_compose(
        vram, sizeof(vram), frame, SPACE_INVADERS_FRAME_WIDTH,
        SPACE_INVADERS_VIDEO_CLASSIC_OVERLAY));
    CHECK(frame[0 * SPACE_INVADERS_FRAME_WIDTH + 55] == 0xffffu);
    CHECK(frame[221 * SPACE_INVADERS_FRAME_WIDTH + 65] == 0x07e0u);
    CHECK(frame[52 * SPACE_INVADERS_FRAME_WIDTH + 149] == 0xf800u);
    CHECK(frame[14 * SPACE_INVADERS_FRAME_WIDTH + 69] == 0xffffu);
    CHECK(frame[0] == 0u && frame[54] == 0u && frame[265] == 0u &&
          frame[319] == 0u);

    CHECK(space_invaders_video_compose(
        vram, sizeof(vram), frame, SPACE_INVADERS_FRAME_WIDTH,
        SPACE_INVADERS_VIDEO_MONO));
    CHECK(frame[221 * SPACE_INVADERS_FRAME_WIDTH + 65] == 0xffffu);
    CHECK(frame[52 * SPACE_INVADERS_FRAME_WIDTH + 149] == 0xffffu);

    if (failures != 0u) return 1;
    puts("Space Invaders video adapter checks passed");
    return 0;
}
