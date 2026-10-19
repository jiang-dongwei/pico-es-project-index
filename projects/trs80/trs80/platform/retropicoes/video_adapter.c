/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "video_adapter.h"
extern const unsigned char trs_char_data[][256][12];
int trs80_render_video(uint16_t *pixels, size_t stride, unsigned width, unsigned height,
                       trs80_model_t model, int scanlines)
{
    trs80_status_t s = trs80_status();
    size_t size;
    const uint8_t *video = trs80_video(&size);
    if (!pixels || !width || !height || stride < width) return 0;
    unsigned native_width = s.columns * 8u, native_height = s.rows * 12u;
    unsigned charset = model == TRS80_MODEL_I ? 3u : (model == TRS80_MODEL_III ? 4u : 8u);
    uint8_t line[640];
    if (native_width > sizeof(line)) return 0;
    unsigned advance = native_width / width, remainder = native_width % width;
    unsigned cell_step = s.columns == 32u || s.columns == 40u ? 2u : 1u;
    unsigned previous_sy = ~0u;
    for (unsigned y = 0; y < height; ++y) {
        unsigned sy = y * native_height / height, row = sy / 12u, gy = sy % 12u;
        if (sy != previous_sy) {
            for (unsigned column = 0; column < s.columns; ++column) {
                size_t offset = s.video_offset + (row * s.columns + column) * cell_step;
                unsigned c = offset < size ? video[offset] : 32u;
                if (model == TRS80_MODEL_I && c >= 0xc0u) c -= 0x40u;
                if (model != TRS80_MODEL_I && c >= 0xc0u && !s.alternate && !s.inverse) c -= 0x40u;
                unsigned bits;
                if (c >= 0x80u && c <= 0xbfu && !s.inverse) {
                    unsigned blocks = (c >> ((gy / 4u) * 2u)) & 3u;
                    bits = (blocks & 1u ? 0x0fu : 0u) | (blocks & 2u ? 0xf0u : 0u);
                } else {
                    unsigned inv = s.inverse && (c & 0x80u);
                    if (s.inverse) c &= 0x7fu;
                    bits = trs_char_data[charset][c][gy] ^ (inv ? 0xffu : 0u);
                }
                for (unsigned gx = 0; gx < 8u; ++gx) line[column * 8u + gx] = (bits >> gx) & 1u;
            }
            previous_sy = sy;
        }
        unsigned first = 0, fraction = 0;
        uint16_t color = scanlines && (y & 1u) ? 0x7befu : 0xffffu;
        uint16_t *out = pixels + y * stride;
        for (unsigned x = 0; x < width; ++x) {
            unsigned next = first + advance;
            fraction += remainder;
            if (fraction >= width) { fraction -= width; ++next; }
            unsigned last = next > first ? next : first + 1u;
            unsigned lit = 0;
            for (unsigned sx = first; sx < last; ++sx) lit |= line[sx];
            out[x] = lit ? color : 0u;
            first = next;
        }
    }
    return 1;
}
