/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_PERF_REPORT_H
#define RETROPICOES_PERF_REPORT_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "video_backend.h"

/* Core-0 only. No allocation, IRQ changes, FIFO waits or per-frame printf.
 * Timing includes reporting and pacing across consecutive wall-time windows.
 * Source audio starvation and write overflow are separate sample counters.
 * Display FPS counts framebuffer swaps, not repeated HSTX refreshes. */
class RetroPerfReport {
public:
    bool idle() const { return length_ == 0; }
    // Optional diagnostics share the same queue: never interleave UART bytes
    // with an unfinished PERF record, and never wait for space.
    bool queue_diagnostic(const char *record) {
        if (!idle() || record == nullptr) return false;
        const size_t size = std::strlen(record);
        if (size == 0 || size >= sizeof(report_)) return false;
        std::memcpy(report_, record, size);
        length_ = static_cast<unsigned>(size);
        offset_ = 0;
        return true;
    }
    void begin(const char *name, uint64_t now, uint32_t budget = 16667u,
               uint32_t target_fps10 = 600u) {
        name_ = name;
        start_ = now;
        budget_ = budget;
        target_fps10_ = target_fps10;
        frames_ = over_ = peak_ = 0;
        for (auto &sum : sums_) sum = 0;
        previous_under_ = previous_drop_ = 0;
        length_ = offset_ = dropped_reports_ = 0;
        retropicoes_video_get_stats(&previous_video_);
    }

    void pump() {
        unsigned burst = 0;
        while (offset_ < length_ && burst < 32u &&
               uart_is_writable(uart_default)) {
            uart_putc_raw(uart_default, report_[offset_++]);
            ++burst;
        }
        if (offset_ == length_) offset_ = length_ = 0;
    }

    template<class Stats> void frame(const Stats &s, uint64_t now) {
        const uint32_t values[] = {s.last_input_us, s.last_step_us,
            s.last_draw_us, s.last_present_us, s.last_audio_us, s.last_frame_us};
        for (unsigned i = 0; i < 6; ++i) sums_[i] += values[i];
        ++frames_;
        if (s.last_frame_us > budget_) ++over_;
        if (s.last_frame_us > peak_) peak_ = s.last_frame_us;
        const uint64_t elapsed = now - start_;
        if (elapsed < 1000000u) return;
        retropicoes_video_stats_t video;
        retropicoes_video_get_stats(&video);
        if (length_ == 0) {
            const unsigned fps10 = rate(frames_, elapsed);
            const unsigned shown10 = rate(video.displayed_frames -
                                            previous_video_.displayed_frames, elapsed);
            const unsigned refresh10 = rate(video.hstx_frames -
                                              previous_video_.hstx_frames, elapsed);
            const int used = std::snprintf(report_, sizeof(report_),
                /* A leading CRLF repairs a partial/non-terminated preceding
                   diagnostic and makes every PERF record start on a new line. */
                "\r\nPERF %s fps=%u.%u target=%u.%u display=%u.%u refresh=%u.%u "
                "avg_us input=%u step=%u draw=%u present=%u audio=%u total=%u "
                // pump() uses raw UART, bypassing stdio's LF-to-CRLF mapping.
                "peak=%u budget=%u over=%u/%u\r\n"
                "PERF IO %s audio_under=%u audio_drop=%u video_drop=%u "
                "resync=%u uart_drop=%u\r\n",
                name_, fps10 / 10u, fps10 % 10u,
                target_fps10_ / 10u, target_fps10_ % 10u,
                shown10 / 10u, shown10 % 10u,
                refresh10 / 10u, refresh10 % 10u,
                average(0), average(1), average(2), average(3), average(4),
                average(5), peak_, budget_, over_, frames_, name_,
                s.audio_samples_underflow - previous_under_,
                s.audio_samples_dropped - previous_drop_,
                video.dropped_frames - previous_video_.dropped_frames,
                video.resync_count - previous_video_.resync_count, dropped_reports_);
            if (used > 0 && static_cast<unsigned>(used) < sizeof(report_))
                length_ = static_cast<unsigned>(used);
            else ++dropped_reports_;
        } else ++dropped_reports_;
        previous_video_ = video;
        previous_under_ = s.audio_samples_underflow;
        previous_drop_ = s.audio_samples_dropped;
        start_ = now; // Do not reset the clock after formatting or UART output.
        frames_ = over_ = peak_ = 0;
        for (auto &sum : sums_) sum = 0;
    }

private:
    static unsigned rate(uint32_t count, uint64_t elapsed) {
        return static_cast<unsigned>(static_cast<uint64_t>(count) * 10000000u / elapsed);
    }
    unsigned average(unsigned index) const {
        return static_cast<unsigned>(sums_[index] / frames_);
    }
    const char *name_ = "?";
    uint64_t start_ = 0, sums_[6] = {};
    uint32_t budget_ = 16667, target_fps10_ = 600;
    uint32_t frames_ = 0, over_ = 0, peak_ = 0;
    uint32_t previous_under_ = 0, previous_drop_ = 0;
    retropicoes_video_stats_t previous_video_ = {};
    char report_[512] = {};
    unsigned length_ = 0, offset_ = 0, dropped_reports_ = 0;
};
#endif
