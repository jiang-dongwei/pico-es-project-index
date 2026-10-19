#include "platform_contract.h"

#include <limits.h>
#include <string.h>

static uint64_t add_saturating(uint64_t value, uint32_t increment) {
    if (UINT64_MAX - value < increment) {
        return UINT64_MAX;
    }
    return value + increment;
}

pico286_memory_region_t pico286_classify_address(uint32_t address,
                                                  bool a20_enabled) {
    if (!a20_enabled) {
        address &= 0xFFFFFu;
    }
    if (address < PICO286_CONVENTIONAL_RAM_SIZE) {
        return PICO286_MEMORY_CONVENTIONAL;
    }
    if (address >= PICO286_VIDEO_MEMORY_START &&
        address < PICO286_VIDEO_MEMORY_END) {
        return PICO286_MEMORY_VIDEO;
    }
    if (address >= PICO286_EMS_WINDOW_START &&
        address < PICO286_EMS_WINDOW_END) {
        return PICO286_MEMORY_EMS_WINDOW;
    }
    if (address >= PICO286_UMB_START && address < PICO286_UMB_END) {
        return PICO286_MEMORY_UMB;
    }
    if (address >= PICO286_BIOS_START && address < PICO286_HMA_START) {
        return PICO286_MEMORY_BIOS;
    }
    if (address >= PICO286_HMA_START && address < PICO286_HMA_END) {
        return PICO286_MEMORY_HMA;
    }
    return PICO286_MEMORY_UNMAPPED;
}

bool pico286_clock_init(pico286_clock_t *clock,
                        uint64_t now_us,
                        uint32_t pit_period_us) {
    if (clock == NULL || pit_period_us == 0u) {
        return false;
    }
    memset(clock, 0, sizeof(*clock));
    clock->last_poll_us = now_us;
    clock->next_pit_us = add_saturating(now_us, pit_period_us);
    clock->next_display_us = add_saturating(now_us,
                                             PICO286_DISPLAY_PERIOD_US);
    clock->pit_period_us = pit_period_us;
    clock->initialized = true;
    return true;
}

bool pico286_clock_set_pit_period(pico286_clock_t *clock,
                                  uint64_t now_us,
                                  uint32_t pit_period_us) {
    if (clock == NULL || !clock->initialized || pit_period_us == 0u) {
        return false;
    }
    clock->pit_period_us = pit_period_us;
    clock->next_pit_us = add_saturating(now_us, pit_period_us);
    return true;
}

pico286_clock_events_t pico286_clock_poll(pico286_clock_t *clock,
                                          uint64_t now_us) {
    pico286_clock_events_t events = {0};
    if (clock == NULL || !clock->initialized) {
        events.resynced = true;
        return events;
    }

    if (now_us < clock->last_poll_us) {
        clock->last_poll_us = now_us;
        clock->next_pit_us = add_saturating(now_us, clock->pit_period_us);
        clock->next_display_us = add_saturating(now_us,
                                                 PICO286_DISPLAY_PERIOD_US);
        clock->resync_count++;
        events.resynced = true;
        return events;
    }
    clock->last_poll_us = now_us;

    while (now_us >= clock->next_pit_us &&
           events.pit_ticks < PICO286_MAX_PIT_CATCHUP) {
        events.pit_ticks++;
        clock->next_pit_us = add_saturating(clock->next_pit_us,
                                             clock->pit_period_us);
    }
    if (now_us >= clock->next_pit_us) {
        clock->next_pit_us = add_saturating(now_us, clock->pit_period_us);
        clock->resync_count++;
        events.resynced = true;
    }

    if (now_us >= clock->next_display_us) {
        const uint64_t lateness = now_us - clock->next_display_us;
        const uint64_t elapsed_periods =
            lateness / PICO286_DISPLAY_PERIOD_US;
        events.display_due = true;
        events.video_frames_missed = elapsed_periods > UINT32_MAX
                                         ? UINT32_MAX
                                         : (uint32_t)elapsed_periods;
        if (elapsed_periods >= UINT32_MAX) {
            clock->next_display_us = add_saturating(
                now_us, PICO286_DISPLAY_PERIOD_US);
            clock->resync_count++;
            events.resynced = true;
        } else {
            const uint64_t advance =
                (elapsed_periods + 1u) * PICO286_DISPLAY_PERIOD_US;
            clock->next_display_us = UINT64_MAX - clock->next_display_us < advance
                                         ? UINT64_MAX
                                         : clock->next_display_us + advance;
        }
    }
    return events;
}

bool pico286_describe_video_mode(uint8_t bios_mode,
                                 pico286_video_mode_t *description) {
    if (description == NULL) {
        return false;
    }
    pico286_video_mode_t mode = {
        .target_width = 320u,
        .target_height = 200u,
        .target_x = 0u,
        .target_y = 20u,
        .palette_entries = 16u,
    };

    switch (bios_mode) {
        case 0x00u:
        case 0x01u:
        case 0x02u:
        case 0x03u:
            mode.source_width = bios_mode < 0x02u ? 320u : 640u;
            mode.source_height = 400u;
            break;
        case 0x04u:
        case 0x05u:
            mode.source_width = 320u;
            mode.source_height = 200u;
            mode.palette_entries = 4u;
            break;
        case 0x06u:
            mode.source_width = 640u;
            mode.source_height = 200u;
            mode.palette_entries = 2u;
            break;
        case 0x0Du:
            mode.source_width = 320u;
            mode.source_height = 200u;
            break;
        case 0x0Eu:
            mode.source_width = 640u;
            mode.source_height = 200u;
            break;
        case 0x10u:
            mode.source_width = 640u;
            mode.source_height = 350u;
            mode.target_height = 240u;
            mode.target_y = 0u;
            break;
        case 0x11u:
            mode.source_width = 640u;
            mode.source_height = 480u;
            mode.target_height = 240u;
            mode.target_y = 0u;
            mode.palette_entries = 2u;
            break;
        case 0x12u:
            mode.source_width = 640u;
            mode.source_height = 480u;
            mode.target_height = 240u;
            mode.target_y = 0u;
            break;
        case 0x13u:
        case 0xFFu:
            mode.source_width = 320u;
            mode.source_height = 200u;
            mode.palette_entries = 256u;
            break;
        default:
            return false;
    }

    *description = mode;
    return true;
}

uint16_t pico286_rgb888_to_rgb565(uint32_t rgb888) {
    const uint16_t red = (uint16_t)((rgb888 >> 19u) & 0x1Fu);
    const uint16_t green = (uint16_t)((rgb888 >> 10u) & 0x3Fu);
    const uint16_t blue = (uint16_t)((rgb888 >> 3u) & 0x1Fu);
    return (uint16_t)((red << 11u) | (green << 5u) | blue);
}
