#include "audio.h"
#include "compat.h"

#include <limits.h>
#include <string.h>

static uint32_t load_acquire(const uint32_t *value)
{
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static uint32_t load_relaxed(const uint32_t *value)
{
    return __atomic_load_n(value, __ATOMIC_RELAXED);
}

static void store_release(uint32_t *value, uint32_t next)
{
    __atomic_store_n(value, next, __ATOMIC_RELEASE);
}

int compat_audio_ring_init(compat_audio_ring_t *ring, int16_t *storage,
                           size_t capacity)
{
    if (ring == NULL || storage == NULL || capacity == 0 ||
        capacity > UINT32_MAX / 2u) {
        return -1;
    }
    ring->samples = storage;
    ring->capacity = (uint32_t)capacity;
    ring->read_position = 0;
    ring->write_position = 0;
    return 0;
}

void compat_audio_ring_reset(compat_audio_ring_t *ring)
{
    if (ring == NULL) return;
    __atomic_store_n(&ring->read_position, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&ring->write_position, 0, __ATOMIC_RELAXED);
}

size_t compat_audio_ring_available(const compat_audio_ring_t *ring)
{
    uint32_t write_position;
    uint32_t read_position;
    if (ring == NULL || ring->capacity == 0) return 0;
    write_position = load_acquire(&ring->write_position);
    read_position = load_acquire(&ring->read_position);
    return (size_t)(write_position - read_position);
}

size_t compat_audio_ring_free(const compat_audio_ring_t *ring)
{
    size_t available;
    if (ring == NULL || ring->capacity == 0) return 0;
    available = compat_audio_ring_available(ring);
    if (available >= ring->capacity) return 0;
    return (size_t)ring->capacity - available;
}

COMPAT_HOT(compat_audio_ring_write)
size_t compat_audio_ring_write(compat_audio_ring_t *ring,
                               const int16_t *samples, size_t count)
{
    uint32_t write_position;
    uint32_t read_position;
    size_t writable;
    size_t index;
    if (ring == NULL || samples == NULL || ring->capacity == 0) return 0;
    write_position = load_relaxed(&ring->write_position);
    read_position = load_acquire(&ring->read_position);
    writable = (size_t)ring->capacity - (write_position - read_position);
    if (count > writable) count = writable;
    for (index = 0; index < count; ++index) {
        ring->samples[(write_position + (uint32_t)index) % ring->capacity] =
            samples[index];
    }
    store_release(&ring->write_position, write_position + (uint32_t)count);
    return count;
}

COMPAT_HOT(compat_audio_ring_read)
size_t compat_audio_ring_read(compat_audio_ring_t *ring, int16_t *samples,
                              size_t count)
{
    uint32_t write_position;
    uint32_t read_position;
    size_t readable;
    size_t index;
    if (ring == NULL || samples == NULL || ring->capacity == 0) return 0;
    read_position = load_relaxed(&ring->read_position);
    write_position = load_acquire(&ring->write_position);
    readable = write_position - read_position;
    if (readable > count) readable = count;
    for (index = 0; index < readable; ++index) {
        samples[index] =
            ring->samples[(read_position + (uint32_t)index) % ring->capacity];
    }
    store_release(&ring->read_position, read_position + (uint32_t)readable);
    if (readable < count) {
        memset(samples + readable, 0, (count - readable) * sizeof(*samples));
    }
    return readable;
}
