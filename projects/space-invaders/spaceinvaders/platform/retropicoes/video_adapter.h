/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_SPACE_INVADERS_VIDEO_ADAPTER_H
#define RETROPICOES_SPACE_INVADERS_VIDEO_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SPACE_INVADERS_NATIVE_WIDTH 224u
#define SPACE_INVADERS_NATIVE_HEIGHT 256u
#define SPACE_INVADERS_TARGET_WIDTH 210u
#define SPACE_INVADERS_TARGET_HEIGHT 240u
#define SPACE_INVADERS_FRAME_WIDTH 320u
#define SPACE_INVADERS_FRAME_HEIGHT 240u
#define SPACE_INVADERS_FRAME_X 55u
#define SPACE_INVADERS_VRAM_BYTES 0x1c00u

typedef enum {
    SPACE_INVADERS_VIDEO_MONO = 0,
    SPACE_INVADERS_VIDEO_CLASSIC_OVERLAY
} space_invaders_video_mode_t;

bool space_invaders_video_compose(
    const uint8_t *vram, size_t vram_size,
    uint16_t *frame, unsigned stride,
    space_invaders_video_mode_t mode);

#ifdef __cplusplus
}
#endif
#endif
