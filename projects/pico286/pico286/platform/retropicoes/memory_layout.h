#ifndef PICO286_MEMORY_LAYOUT_H
#define PICO286_MEMORY_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

enum {
    PICO286_PSRAM_CONVENTIONAL_BYTES = 640u * 1024u,
    PICO286_PSRAM_UMB_BYTES = 0xFC000u - 0xD0000u,
    PICO286_PSRAM_HMA_BYTES = 0x110000u - 16u - 0x100000u,
    PICO286_PSRAM_EMS_BYTES = 2u * 1024u * 1024u,
    PICO286_PSRAM_XMS_BYTES = 4u * 1024u * 1024u,
    PICO286_PSRAM_EMULATOR_BYTES =
        PICO286_PSRAM_CONVENTIONAL_BYTES + PICO286_PSRAM_UMB_BYTES +
        PICO286_PSRAM_HMA_BYTES + PICO286_PSRAM_EMS_BYTES +
        PICO286_PSRAM_XMS_BYTES
};

typedef struct pico286_memory_footprint {
    uint32_t conventional_bytes;
    uint32_t umb_bytes;
    uint32_t hma_bytes;
    uint32_t ems_bytes;
    uint32_t xms_bytes;
} pico286_memory_footprint_t;

extern const pico286_memory_footprint_t pico286_upstream_memory_footprint;

bool pico286_memory_footprint_total(
    const pico286_memory_footprint_t *footprint,
    uint32_t *total_bytes);
bool pico286_memory_fits_psram(
    const pico286_memory_footprint_t *footprint,
    uint32_t psram_bytes,
    uint32_t *headroom_bytes);

#endif
