/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "arcade_frame_handoff.h"

#include <stdio.h>

static unsigned failures;
#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

int main(void)
{
    arcade_frame_handoff_t handoff;
    uint16_t frame0[4] = {0};
    uint16_t frame1[4] = {0};
    void *write_frame;

    CHECK(!arcade_frame_handoff_init(NULL, frame0, frame1));
    CHECK(!arcade_frame_handoff_init(&handoff, frame0, frame0));
    CHECK(arcade_frame_handoff_init(&handoff, frame0, frame1));
    CHECK(arcade_frame_handoff_active(&handoff) == frame0);
    write_frame = arcade_frame_handoff_claim(&handoff);
    CHECK(write_frame == frame1);
    CHECK(arcade_frame_handoff_claim(&handoff) == NULL);
    CHECK(atomic_load(&handoff.dropped_frames) == 1u);
    CHECK(arcade_frame_handoff_publish(&handoff));
    CHECK(!arcade_frame_handoff_publish(&handoff));
    CHECK(arcade_frame_handoff_claim(&handoff) == NULL);
    CHECK(arcade_frame_handoff_vsync(&handoff) == frame1);
    CHECK(arcade_frame_handoff_active(&handoff) == frame1);
    CHECK(atomic_load(&handoff.displayed_frames) == 1u);
    CHECK(arcade_frame_handoff_vsync(&handoff) == frame1);
    write_frame = arcade_frame_handoff_claim(&handoff);
    CHECK(write_frame == frame0);
    CHECK(arcade_frame_handoff_publish(&handoff));
    CHECK(arcade_frame_handoff_vsync(&handoff) == frame0);
    CHECK(atomic_load(&handoff.displayed_frames) == 2u);

    if (failures != 0u) return 1;
    puts("Arcade SRAM frame handoff checks passed");
    return 0;
}
