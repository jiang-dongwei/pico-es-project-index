/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "perf_report.h"

#include <stdio.h>
#include <string.h>

static unsigned failures;
#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

int main(void)
{
    space_invaders_perf_t perf;
    space_invaders_perf_frame_t frame = {
        200u, 9000u, 5000u, 100u, 500u, 14800u,
        1u, 1u, 0u, 0u, 0u, 0u
    };
    char report[512];
    size_t length;

    space_invaders_perf_init(&perf, 100u);
    for (unsigned i = 0; i < 60u; ++i)
        space_invaders_perf_add(&perf, &frame);
    CHECK(space_invaders_perf_format(&perf, &frame, 999999u,
                                     report, sizeof(report)) == 0u);
    frame.audio_underflow = 2u;
    frame.video_dropped = 1u;
    space_invaders_perf_note_uart_drop(&perf);
    length = space_invaders_perf_format(&perf, &frame, 1000100u,
                                        report, sizeof(report));
    CHECK(length != 0u && length == strlen(report));
    CHECK(strstr(report,
        "\r\nPERF SPACEINVADERS fps=60.0 target=60.0 display=60.0 refresh=60.0")
          == report);
    CHECK(strstr(report,
        "avg_us input=200 step=9000 draw=5000 present=100 audio=500 total=14800")
          != NULL);
    CHECK(strstr(report, "peak=14800 budget=16667 over=0/60\r\n") != NULL);
    CHECK(strstr(report,
        "PERF IO SPACEINVADERS audio_under=2 audio_drop=0 video_drop=1 "
        "resync=0 uart_drop=1\r\n") != NULL);
    CHECK(report[length - 2u] == '\r' && report[length - 1u] == '\n');

    space_invaders_perf_add(&perf, &frame);
    CHECK(space_invaders_perf_format(&perf, &frame, 2000100u,
                                     report, 16u) == 0u);
    CHECK(perf.uart_dropped == 1u);

    if (failures != 0u) return 1;
    puts("Space Invaders performance report checks passed");
    return 0;
}
