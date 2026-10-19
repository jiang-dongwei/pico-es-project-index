#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "pico/platform.h"
#include "pico/time.h"

#include "compat.h"
#include "hstx_data_island_queue.h"
#include "runtime.h"
#include "video_backend.h"
#include "video_output.h"

#if !defined(RETROPICOES_HSTX_RGB565)
#error "RetroPicoES framebuffer requires the pico_hdmi HSTX RGB565 encoder"
#endif

#ifndef FORCE_DVI
#define FORCE_DVI 0
#endif

/* Opt in per port: preserve a pending direct-render frame until VSYNC.
   Default retains the existing low-latency replacement policy for other ports. */
#ifndef RETROPICOES_WAIT_READY_US
#define RETROPICOES_WAIT_READY_US 0
#endif

enum {
    FRAME_PIXELS = COMPAT_SCREEN_WIDTH * COMPAT_SCREEN_HEIGHT,
    FRAME_BYTES = FRAME_PIXELS * sizeof(compat_pixel_t),
    FRAME_STATE_ACTIVE = 1u << 0,
    FRAME_STATE_READY = 1u << 1,
    FRAME_STATE_COPYING = 1u << 2,
    HSTX_SIGNAL_WIDTH = COMPAT_SCREEN_WIDTH * 2u,
    HSTX_SIGNAL_HEIGHT = COMPAT_SCREEN_HEIGHT * 2u,
    CORE1_STACK_WORDS = 1024u
};

static const uint32_t kHstxClockHz = 126000000u;

/*
 * HSTX scans only the active SRAM buffer. Core 0 renders into PSRAM and
 * copies into the inactive SRAM buffer without ever waiting for VSYNC. If a
 * second frame arrives before the pending one is displayed, it replaces that
 * pending frame; therefore the queue always contains at most the newest frame.
 */
static compat_pixel_t g_scanout[2][FRAME_PIXELS]
    __attribute__((aligned(4)));
static uint32_t g_core1_stack[CORE1_STACK_WORDS]
    __attribute__((aligned(8)));
static compat_pixel_t *g_draw_pixels;
static compat_framebuffer_t g_framebuffer;
static _Atomic uint32_t g_frame_state;
static _Atomic uint32_t g_published_frames;
static _Atomic uint32_t g_displayed_frames;
static _Atomic uint32_t g_dropped_frames;
static bool g_video_ready;
#if defined(RETROPICOES_DIRECT_SRAM_RENDER) && RETROPICOES_DIRECT_SRAM_RENDER
static bool g_direct_frame_claimed;
#endif

static void __not_in_flash_func(retropicoes_scanline_callback)(
    uint32_t v_scanline, uint32_t active_line, uint32_t *line_buffer)
{
    uint32_t state;
    const uint32_t *source;
    uint32_t x;

    (void)v_scanline;
    if (active_line >= HSTX_SIGNAL_HEIGHT) {
        for (x = 0; x < HSTX_SIGNAL_WIDTH / 2u; ++x) {
            line_buffer[x] = 0u;
        }
        return;
    }

    state = atomic_load_explicit(&g_frame_state, memory_order_acquire);
    source = (const uint32_t *)&g_scanout[state & FRAME_STATE_ACTIVE]
                                      [(active_line >> 1) *
                                       COMPAT_SCREEN_WIDTH];

    /* Use pico_shared's validated 640x480 signal path. Each source line is
       scanned twice, and each packed pair of 320-wide RGB565 source pixels
       becomes two words containing two horizontal copies of each pixel. */
    for (x = 0; x < COMPAT_SCREEN_WIDTH / 2u; ++x) {
        uint32_t source_pair = source[x];
        uint32_t pixel0 = source_pair & 0xffffu;
        uint32_t pixel1 = source_pair >> 16;
        uint32_t pair0 = pixel0 | (pixel0 << 16);
        uint32_t pair1 = pixel1 | (pixel1 << 16);
        line_buffer[x * 2u] = pair0;
        line_buffer[x * 2u + 1u] = pair1;
    }
}

static void __not_in_flash_func(retropicoes_vsync_callback)(void)
{
    uint32_t state =
        atomic_load_explicit(&g_frame_state, memory_order_acquire);

    while ((state & (FRAME_STATE_READY | FRAME_STATE_COPYING)) ==
           FRAME_STATE_READY) {
        uint32_t desired =
            (state ^ FRAME_STATE_ACTIVE) & ~FRAME_STATE_READY;
        if (atomic_compare_exchange_weak_explicit(
                &g_frame_state, &state, desired, memory_order_acq_rel,
                memory_order_acquire)) {
            atomic_fetch_add_explicit(&g_displayed_frames, 1u,
                                      memory_order_relaxed);
            break;
        }
    }
}

int retropicoes_video_init(void)
{
    uint32_t system_clock_hz;
    uint32_t hstx_divider;

    if (g_video_ready) {
        return 0;
    }
    if (!retropicoes_psram_ready()) {
        return -1;
    }

#if defined(RETROPICOES_DIRECT_SRAM_RENDER) && RETROPICOES_DIRECT_SRAM_RENDER
    g_draw_pixels = NULL;
    g_direct_frame_claimed = false;
#else
    g_draw_pixels = (compat_pixel_t *)compat_calloc(FRAME_PIXELS,
                                                     sizeof(*g_draw_pixels));
    if (g_draw_pixels == NULL ||
        !retropicoes_pointer_in_psram(g_draw_pixels)) {
        compat_free(g_draw_pixels);
        g_draw_pixels = NULL;
        return -2;
    }
#endif

    memset(g_scanout, 0, sizeof(g_scanout));
#if defined(RETROPICOES_DIRECT_SRAM_RENDER) && RETROPICOES_DIRECT_SRAM_RENDER
    g_framebuffer.pixels = g_scanout[1];
#else
    g_framebuffer.pixels = g_draw_pixels;
#endif
    g_framebuffer.stride = COMPAT_SCREEN_WIDTH;
    g_framebuffer.width = COMPAT_SCREEN_WIDTH;
    g_framebuffer.height = COMPAT_SCREEN_HEIGHT;
    atomic_store_explicit(&g_frame_state, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_published_frames, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_displayed_frames, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_dropped_frames, 0u, memory_order_relaxed);

    system_clock_hz = clock_get_hz(clk_sys);
    if (system_clock_hz < kHstxClockHz ||
        system_clock_hz % kHstxClockHz != 0u) {
        compat_free(g_draw_pixels);
        g_draw_pixels = NULL;
        memset(&g_framebuffer, 0, sizeof(g_framebuffer));
        return -3;
    }
    hstx_divider = system_clock_hz / kHstxClockHz;
    clock_configure_int_divider(
        clk_hstx, 0u, CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLK_SYS,
        system_clock_hz, hstx_divider);
    if (clock_get_hz(clk_hstx) != kHstxClockHz) {
        compat_free(g_draw_pixels);
        g_draw_pixels = NULL;
        memset(&g_framebuffer, 0, sizeof(g_framebuffer));
        return -4;
    }

    hstx_di_queue_init();
    video_output_set_dvi_mode(FORCE_DVI != 0);
    video_output_set_vsync_callback(retropicoes_vsync_callback);
    video_output_init(HSTX_SIGNAL_WIDTH, HSTX_SIGNAL_HEIGHT);
    video_output_set_scanline_callback(retropicoes_scanline_callback);

    g_video_ready = true;
    multicore_launch_core1_with_stack(video_output_core1_run,
                                      g_core1_stack,
                                      sizeof(g_core1_stack));
    printf("HSTX clock: sys=%u Hz divider=%u hstx=%u Hz\n",
           (unsigned)system_clock_hz, (unsigned)hstx_divider,
           (unsigned)clock_get_hz(clk_hstx));
    return 0;
}

void retropicoes_video_shutdown(void)
{
    if (!g_video_ready) {
        return;
    }

    video_output_set_background_task(NULL);
    video_output_stop();
    multicore_reset_core1();
    g_video_ready = false;
    compat_free(g_draw_pixels);
    g_draw_pixels = NULL;
    memset(&g_framebuffer, 0, sizeof(g_framebuffer));
}

int retropicoes_video_ready(void)
{
    return g_video_ready ? 1 : 0;
}

compat_framebuffer_t *compat_framebuffer(void)
{
#if defined(RETROPICOES_DIRECT_SRAM_RENDER) && RETROPICOES_DIRECT_SRAM_RENDER
    uint32_t state;
    uint32_t desired;
    uint32_t target;
    if (!g_video_ready) return NULL;
    if (g_direct_frame_claimed) return &g_framebuffer;
    state = atomic_load_explicit(&g_frame_state, memory_order_acquire);
#if RETROPICOES_WAIT_READY_US > 0
    const uint32_t wait_start = time_us_32();
    while ((state & FRAME_STATE_READY) != 0u &&
           (uint32_t)(time_us_32() - wait_start) < RETROPICOES_WAIT_READY_US) {
        /* Never claim COPYING while waiting: core 1 must be able to swap.
           A stopped display times out and uses the counted replacement path. */
        tight_loop_contents();
        state = atomic_load_explicit(&g_frame_state, memory_order_acquire);
    }
#endif
    do {
        desired = (state | FRAME_STATE_COPYING) & ~FRAME_STATE_READY;
    } while (!atomic_compare_exchange_weak_explicit(
        &g_frame_state, &state, desired, memory_order_acq_rel,
        memory_order_acquire));
    if ((state & FRAME_STATE_READY) != 0u)
        atomic_fetch_add_explicit(&g_dropped_frames, 1u,
                                  memory_order_relaxed);
    target = (state & FRAME_STATE_ACTIVE) ^ FRAME_STATE_ACTIVE;
    g_framebuffer.pixels = g_scanout[target];
    g_direct_frame_claimed = true;
    return &g_framebuffer;
#else
    return g_video_ready ? &g_framebuffer : NULL;
#endif
}

COMPAT_HOT(compat_present)
void compat_present(void)
{
    uint32_t state;
    uint32_t desired;
    uint32_t target;

    if (!g_video_ready) {
        return;
    }

#if defined(RETROPICOES_DIRECT_SRAM_RENDER) && RETROPICOES_DIRECT_SRAM_RENDER
    if (!g_direct_frame_claimed) return;
    state = atomic_load_explicit(&g_frame_state, memory_order_acquire);
    atomic_fetch_add_explicit(&g_published_frames, 1u,
                              memory_order_relaxed);
    atomic_store_explicit(&g_frame_state,
                          (state & FRAME_STATE_ACTIVE) | FRAME_STATE_READY,
                          memory_order_release);
    g_direct_frame_claimed = false;
    return;
#else
    if (g_draw_pixels == NULL) return;

    state = atomic_load_explicit(&g_frame_state, memory_order_acquire);
    do {
        desired = (state | FRAME_STATE_COPYING) & ~FRAME_STATE_READY;
    } while (!atomic_compare_exchange_weak_explicit(
        &g_frame_state, &state, desired, memory_order_acq_rel,
        memory_order_acquire));

    if ((state & FRAME_STATE_READY) != 0u) {
        atomic_fetch_add_explicit(&g_dropped_frames, 1u,
                                  memory_order_relaxed);
    }
    target = (state & FRAME_STATE_ACTIVE) ^ FRAME_STATE_ACTIVE;
    memcpy(g_scanout[target], g_draw_pixels, FRAME_BYTES);
    atomic_fetch_add_explicit(&g_published_frames, 1u,
                              memory_order_relaxed);

    /* COPYING prevents the VSYNC callback from selecting the target while
       memcpy is in progress. Publishing READY releases the complete frame. */
    atomic_store_explicit(&g_frame_state,
                          (state & FRAME_STATE_ACTIVE) | FRAME_STATE_READY,
                          memory_order_release);
#endif
}

void retropicoes_video_get_stats(retropicoes_video_stats_t *stats)
{
    if (stats == NULL) {
        return;
    }
    stats->hstx_frames = video_frame_count;
    stats->published_frames = atomic_load_explicit(
        &g_published_frames, memory_order_relaxed);
    stats->displayed_frames = atomic_load_explicit(
        &g_displayed_frames, memory_order_relaxed);
    stats->dropped_frames = atomic_load_explicit(
        &g_dropped_frames, memory_order_relaxed);
    stats->resync_count = (uint32_t)get_video_output_resync_count();
}
