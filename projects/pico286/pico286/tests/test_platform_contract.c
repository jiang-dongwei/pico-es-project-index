#include "platform_contract.h"

#include <assert.h>
#include <stdint.h>

static void test_memory_map(void) {
    assert(pico286_classify_address(0u, true) == PICO286_MEMORY_CONVENTIONAL);
    assert(pico286_classify_address(0x9FFFFu, true) == PICO286_MEMORY_CONVENTIONAL);
    assert(pico286_classify_address(0xA0000u, true) == PICO286_MEMORY_VIDEO);
    assert(pico286_classify_address(0xBFFFFu, true) == PICO286_MEMORY_VIDEO);
    assert(pico286_classify_address(0xC0000u, true) == PICO286_MEMORY_EMS_WINDOW);
    assert(pico286_classify_address(0xD0000u, true) == PICO286_MEMORY_UMB);
    assert(pico286_classify_address(0xFC000u, true) == PICO286_MEMORY_UNMAPPED);
    assert(pico286_classify_address(0xFE000u, true) == PICO286_MEMORY_BIOS);
    assert(pico286_classify_address(0x100000u, true) == PICO286_MEMORY_HMA);
    assert(pico286_classify_address(0x10FFFFu, true) == PICO286_MEMORY_UNMAPPED);
    assert(pico286_classify_address(0x100123u, false) ==
           PICO286_MEMORY_CONVENTIONAL);
}

static void test_clock(void) {
    pico286_clock_t clock;
    assert(!pico286_clock_init(NULL, 0u, 100u));
    assert(!pico286_clock_init(&clock, 0u, 0u));
    assert(pico286_clock_init(&clock, 1000u, 100u));

    pico286_clock_events_t events = pico286_clock_poll(&clock, 1099u);
    assert(events.pit_ticks == 0u);
    assert(!events.display_due);

    events = pico286_clock_poll(&clock, 1100u);
    assert(events.pit_ticks == 1u);

    events = pico286_clock_poll(&clock, 17667u);
    assert(events.pit_ticks == PICO286_MAX_PIT_CATCHUP);
    assert(events.display_due);
    assert(events.resynced);

    assert(pico286_clock_set_pit_period(&clock, 18000u, 1000u));
    events = pico286_clock_poll(&clock, 19000u);
    assert(events.pit_ticks == 1u);

    events = pico286_clock_poll(&clock, 10u);
    assert(events.resynced);
    assert(clock.resync_count >= 2u);
}

static void test_video_modes(void) {
    pico286_video_mode_t mode;
    assert(pico286_describe_video_mode(0x03u, &mode));
    assert(mode.source_width == 640u && mode.source_height == 400u);
    assert(mode.target_width == 320u && mode.target_height == 200u);

    assert(pico286_describe_video_mode(0x04u, &mode));
    assert(mode.source_width == 320u && mode.palette_entries == 4u);

    assert(pico286_describe_video_mode(0x12u, &mode));
    assert(mode.source_width == 640u && mode.source_height == 480u);
    assert(mode.target_height == 240u && mode.target_y == 0u);

    assert(pico286_describe_video_mode(0x13u, &mode));
    assert(mode.source_width == 320u && mode.palette_entries == 256u);
    assert(!pico286_describe_video_mode(0x7Fu, &mode));
    assert(!pico286_describe_video_mode(0x03u, NULL));

    assert(pico286_rgb888_to_rgb565(0xFF0000u) == 0xF800u);
    assert(pico286_rgb888_to_rgb565(0x00FF00u) == 0x07E0u);
    assert(pico286_rgb888_to_rgb565(0x0000FFu) == 0x001Fu);
    assert(pico286_rgb888_to_rgb565(0xFFFFFFu) == 0xFFFFu);
}

int main(void) {
    test_memory_map();
    test_clock();
    test_video_modes();
    return 0;
}
