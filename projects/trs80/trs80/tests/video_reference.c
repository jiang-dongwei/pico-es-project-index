/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "video_adapter.h"
extern const unsigned char trs_char_data[][256][12];
int trs80_render_reference(uint16_t *pixels, size_t stride, unsigned width, unsigned height,
                       trs80_model_t model, int scanlines)
{
    trs80_status_t s = trs80_status();
    size_t size;
    const uint8_t *video = trs80_video(&size);
    if (!pixels || !width || !height || stride < width) return 0;
    unsigned native_width = s.columns * 8u, native_height = s.rows * 12u;
    unsigned charset = model == TRS80_MODEL_I ? 3u : (model == TRS80_MODEL_III ? 4u : 8u);
    for (unsigned y = 0; y < height; ++y) {
        unsigned sy = y * native_height / height, row = sy / 12u, gy = sy % 12u;
        for (unsigned x = 0; x < width; ++x) {
            unsigned first = x * native_width / width;
            unsigned last = (x + 1u) * native_width / width;
            if (last <= first) last = first + 1u;
            int lit = 0;
            for (unsigned sx = first; sx < last; ++sx) {
                int expanded = s.columns == 32u || s.columns == 40u;
                size_t offset = s.video_offset + row * s.columns * (expanded ? 2u : 1u) +
                    (sx / 8u) * (expanded ? 2u : 1u);
                unsigned c = offset < size ? video[offset] : 32u;
                unsigned gx = sx % 8u;
                if (model == TRS80_MODEL_I && c >= 0xc0u) c -= 0x40u;
                if (model != TRS80_MODEL_I && c >= 0xc0u && !s.alternate && !s.inverse) c -= 0x40u;
                if (c >= 0x80u && c <= 0xbfu && !s.inverse)
                    lit |= (c >> ((gy / 4u) * 2u + gx / 4u)) & 1u;
                else {
                    unsigned inv = s.inverse && (c & 0x80u);
                    if (s.inverse) c &= 0x7fu;
                    lit |= ((trs_char_data[charset][c][gy] >> gx) & 1u) ^ (inv != 0u);
                }
            }
            pixels[y * stride + x] = lit ? (scanlines && (y & 1u) ? 0x7befu : 0xffffu) : 0u;
        }
    }
    return 1;
}
