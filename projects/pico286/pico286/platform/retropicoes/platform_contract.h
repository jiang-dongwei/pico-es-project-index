#ifndef PICO286_RETROPICOES_PLATFORM_CONTRACT_H
#define PICO286_RETROPICOES_PLATFORM_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PICO286_CONVENTIONAL_RAM_SIZE = 640u * 1024u,
    PICO286_VIDEO_MEMORY_START = 0xA0000u,
    PICO286_VIDEO_MEMORY_END = 0xC0000u,
    PICO286_EMS_WINDOW_START = 0xC0000u,
    PICO286_EMS_WINDOW_END = 0xD0000u,
    PICO286_UMB_START = 0xD0000u,
    PICO286_UMB_END = 0xFC000u,
    PICO286_BIOS_START = 0xFE000u,
    PICO286_HMA_START = 0x100000u,
    PICO286_HMA_END = 0x10FFF0u,
    PICO286_DISPLAY_PERIOD_US = 16667u,
    PICO286_MAX_PIT_CATCHUP = 8u
};

typedef enum pico286_memory_region {
    PICO286_MEMORY_CONVENTIONAL,
    PICO286_MEMORY_VIDEO,
    PICO286_MEMORY_EMS_WINDOW,
    PICO286_MEMORY_UMB,
    PICO286_MEMORY_BIOS,
    PICO286_MEMORY_HMA,
    PICO286_MEMORY_UNMAPPED
} pico286_memory_region_t;

typedef struct pico286_clock {
    uint64_t last_poll_us;
    uint64_t next_pit_us;
    uint64_t next_display_us;
    uint32_t pit_period_us;
    uint32_t resync_count;
    bool initialized;
} pico286_clock_t;

typedef struct pico286_clock_events {
    uint8_t pit_ticks;
    uint32_t video_frames_missed;
    bool display_due;
    bool resynced;
} pico286_clock_events_t;

typedef struct pico286_video_mode {
    uint16_t source_width;
    uint16_t source_height;
    uint16_t target_width;
    uint16_t target_height;
    uint16_t target_x;
    uint16_t target_y;
    uint16_t palette_entries;
} pico286_video_mode_t;

pico286_memory_region_t pico286_classify_address(uint32_t address,
                                                  bool a20_enabled);

bool pico286_clock_init(pico286_clock_t *clock,
                        uint64_t now_us,
                        uint32_t pit_period_us);
bool pico286_clock_set_pit_period(pico286_clock_t *clock,
                                  uint64_t now_us,
                                  uint32_t pit_period_us);
pico286_clock_events_t pico286_clock_poll(pico286_clock_t *clock,
                                          uint64_t now_us);

bool pico286_describe_video_mode(uint8_t bios_mode,
                                 pico286_video_mode_t *description);
uint16_t pico286_rgb888_to_rgb565(uint32_t rgb888);

#ifdef __cplusplus
}
#endif

#endif
