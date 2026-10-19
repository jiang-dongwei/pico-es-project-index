#ifndef REFKEEN_INDEXED_VIDEO_H
#define REFKEEN_INDEXED_VIDEO_H

#include <stdint.h>

void refkeen_indexed_row_bases(uint32_t out[200], uint32_t start,
                               uint32_t panning, uint32_t stride,
                               int split_line, uint32_t wrap_size);

void refkeen_indexed_blit(const uint8_t *source, uint32_t wrap_size,
                          const uint32_t row_bases[200],
                          const uint16_t palette[256], uint16_t *dest,
                          int dest_stride, int dest_width, int dest_height,
                          int dx, int dy, int dw, int dh);

#endif
