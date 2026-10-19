/* Lock-free single-producer/single-consumer PCM ring for the Pico port. */
#ifndef SDLPOP_COMPAT_AUDIO_H
#define SDLPOP_COMPAT_AUDIO_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int16_t *samples;
    uint32_t capacity;
    uint32_t read_position;
    uint32_t write_position;
} compat_audio_ring_t;

int compat_audio_ring_init(compat_audio_ring_t *ring, int16_t *storage,
                           size_t capacity);
void compat_audio_ring_reset(compat_audio_ring_t *ring);
size_t compat_audio_ring_available(const compat_audio_ring_t *ring);
size_t compat_audio_ring_free(const compat_audio_ring_t *ring);
size_t compat_audio_ring_write(compat_audio_ring_t *ring,
                               const int16_t *samples, size_t count);
/* Returns real samples read; any underrun tail is filled with zeroes. */
size_t compat_audio_ring_read(compat_audio_ring_t *ring, int16_t *samples,
                              size_t count);

#endif
