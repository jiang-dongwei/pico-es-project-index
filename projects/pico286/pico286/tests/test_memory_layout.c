#include "board_profile.h"
#include "memory_layout.h"

#include <assert.h>
#include <limits.h>

int main(void)
{
    pico286_memory_footprint_t footprint = pico286_upstream_memory_footprint;
    uint32_t total = 0u;
    uint32_t headroom = 0u;

    assert(PICO286_PSRAM_EMULATOR_BYTES == 7192560u);
    assert(pico286_memory_footprint_total(&footprint, &total));
    assert(total == PICO286_PSRAM_EMULATOR_BYTES);
    assert(pico286_memory_fits_psram(&footprint,
                                    PICO286_PSRAM_SIZE_BYTES,
                                    &headroom));
    assert(headroom == 1196048u);
    assert(!pico286_memory_fits_psram(&footprint, total - 1u, &headroom));
    assert(!pico286_memory_fits_psram(&footprint, total, NULL));
    assert(!pico286_memory_footprint_total(NULL, &total));
    assert(!pico286_memory_footprint_total(&footprint, NULL));

    footprint.xms_bytes = UINT32_MAX;
    assert(!pico286_memory_footprint_total(&footprint, &total));

    return 0;
}
