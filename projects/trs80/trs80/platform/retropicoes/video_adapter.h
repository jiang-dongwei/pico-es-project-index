/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TRS80_VIDEO_ADAPTER_H
#define TRS80_VIDEO_ADAPTER_H
#include "trs80.h"
int trs80_render_video(uint16_t *pixels, size_t stride, unsigned width, unsigned height,
                       trs80_model_t model, int scanlines);
#endif
