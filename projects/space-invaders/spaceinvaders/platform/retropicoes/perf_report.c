/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "perf_report.h"

#include <stdio.h>
#include <string.h>

#define FRAME_BUDGET_US 16667u

static uint32_t rate10(uint32_t count, uint64_t elapsed_us)
{
    if (elapsed_us == 0u) return 0u;
    return (uint32_t)((uint64_t)count * 10000000u / elapsed_us);
}

static uint32_t average(const space_invaders_perf_t *perf, unsigned index)
{
    return perf->frames == 0u ? 0u :
           (uint32_t)(perf->sums[index] / perf->frames);
}

void space_invaders_perf_init(space_invaders_perf_t *perf, uint64_t now_us)
{
    if (perf == NULL) return;
    memset(perf, 0, sizeof(*perf));
    perf->window_start_us = now_us;
}

void space_invaders_perf_add(space_invaders_perf_t *perf,
                             const space_invaders_perf_frame_t *frame)
{
    if (perf == NULL || frame == NULL) return;
    perf->sums[0] += frame->input_us;
    perf->sums[1] += frame->step_us;
    perf->sums[2] += frame->draw_us;
    perf->sums[3] += frame->present_us;
    perf->sums[4] += frame->audio_us;
    perf->sums[5] += frame->total_us;
    ++perf->frames;
    perf->displayed_frames += frame->displayed_frames;
    perf->refresh_frames += frame->refresh_frames;
    if (frame->total_us > FRAME_BUDGET_US) ++perf->over_budget;
    if (frame->total_us > perf->peak_us) perf->peak_us = frame->total_us;
}

void space_invaders_perf_note_uart_drop(space_invaders_perf_t *perf)
{
    if (perf != NULL) ++perf->uart_dropped;
}

size_t space_invaders_perf_format(space_invaders_perf_t *perf,
                                  const space_invaders_perf_frame_t *latest,
                                  uint64_t now_us,
                                  char *output, size_t capacity)
{
    uint64_t elapsed;
    uint32_t fps10;
    uint32_t display10;
    uint32_t refresh10;
    int used;
    if (perf == NULL || latest == NULL || output == NULL || capacity == 0u ||
        now_us < perf->window_start_us)
        return 0u;
    elapsed = now_us - perf->window_start_us;
    if (elapsed < 1000000u || perf->frames == 0u)
        return 0u;
    fps10 = rate10(perf->frames, elapsed);
    display10 = rate10(perf->displayed_frames, elapsed);
    refresh10 = rate10(perf->refresh_frames, elapsed);
    used = snprintf(output, capacity,
        "\r\nPERF SPACEINVADERS fps=%u.%u target=60.0 display=%u.%u "
        "refresh=%u.%u avg_us input=%u step=%u draw=%u present=%u "
        "audio=%u total=%u peak=%u budget=%u over=%u/%u\r\n"
        "PERF IO SPACEINVADERS audio_under=%u audio_drop=%u "
        "video_drop=%u resync=%u uart_drop=%u\r\n",
        fps10 / 10u, fps10 % 10u,
        display10 / 10u, display10 % 10u,
        refresh10 / 10u, refresh10 % 10u,
        average(perf, 0u), average(perf, 1u), average(perf, 2u),
        average(perf, 3u), average(perf, 4u), average(perf, 5u),
        perf->peak_us, FRAME_BUDGET_US, perf->over_budget, perf->frames,
        latest->audio_underflow - perf->previous_audio_underflow,
        latest->audio_dropped - perf->previous_audio_dropped,
        latest->video_dropped - perf->previous_video_dropped,
        latest->resync_count - perf->previous_resync_count,
        perf->uart_dropped);
    if (used <= 0 || (size_t)used >= capacity) {
        ++perf->uart_dropped;
        return 0u;
    }
    perf->previous_audio_underflow = latest->audio_underflow;
    perf->previous_audio_dropped = latest->audio_dropped;
    perf->previous_video_dropped = latest->video_dropped;
    perf->previous_resync_count = latest->resync_count;
    perf->window_start_us = now_us;
    memset(perf->sums, 0, sizeof(perf->sums));
    perf->frames = 0u;
    perf->displayed_frames = 0u;
    perf->refresh_frames = 0u;
    perf->over_budget = 0u;
    perf->peak_us = 0u;
    perf->uart_dropped = 0u;
    return (size_t)used;
}
