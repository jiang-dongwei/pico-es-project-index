#include "memory_layout.h"

#include <limits.h>
#include <stddef.h>

const pico286_memory_footprint_t pico286_upstream_memory_footprint = {
    .conventional_bytes = PICO286_PSRAM_CONVENTIONAL_BYTES,
    .umb_bytes = PICO286_PSRAM_UMB_BYTES,
    .hma_bytes = PICO286_PSRAM_HMA_BYTES,
    .ems_bytes = PICO286_PSRAM_EMS_BYTES,
    .xms_bytes = PICO286_PSRAM_XMS_BYTES,
};

bool pico286_memory_footprint_total(
    const pico286_memory_footprint_t *footprint,
    uint32_t *total_bytes)
{
    uint32_t total = 0u;
    const uint32_t parts[] = {
        footprint != NULL ? footprint->conventional_bytes : 0u,
        footprint != NULL ? footprint->umb_bytes : 0u,
        footprint != NULL ? footprint->hma_bytes : 0u,
        footprint != NULL ? footprint->ems_bytes : 0u,
        footprint != NULL ? footprint->xms_bytes : 0u,
    };

    if ((footprint == NULL) || (total_bytes == NULL)) {
        return false;
    }
    for (size_t index = 0u; index < sizeof(parts) / sizeof(parts[0]); ++index) {
        if (UINT32_MAX - total < parts[index]) {
            return false;
        }
        total += parts[index];
    }
    *total_bytes = total;
    return true;
}

bool pico286_memory_fits_psram(
    const pico286_memory_footprint_t *footprint,
    uint32_t psram_bytes,
    uint32_t *headroom_bytes)
{
    uint32_t total = 0u;
    if ((headroom_bytes == NULL) ||
        !pico286_memory_footprint_total(footprint, &total) ||
        (total > psram_bytes)) {
        return false;
    }
    *headroom_bytes = psram_bytes - total;
    return true;
}
