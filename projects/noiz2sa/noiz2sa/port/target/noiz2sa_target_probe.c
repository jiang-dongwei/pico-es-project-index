/*
 * S17-D boundary probe for the noiz2sa port.
 *
 * This target links the same noiz2sa_game archive the RP2350B build will use,
 * but pairs it with the host-OS-free stub backend (port/target/compat_stub.c)
 * instead of noiz2sa_host_backend. A successful build and run therefore
 * establishes two things that a comment cannot:
 *
 *   1. If noiz2sa_game carried a host-only dependency, this target would not
 *      link. The boundary is checked by the linker.
 *   2. The replay scenario below is the host smoke's, so the framebuffer
 *      digest must equal noiz2sa_host_smoke's FRAME HASH. A backend swap that
 *      changed behaviour would show up as a different hash.
 *
 * This file uses no host OS API either, so it does not chdir itself: the
 * caller sets the working directory. scripts/test-target-boundary.ps1 does.
 * That keeps noiz2sa_target_probe.exe free of <direct.h>/<unistd.h>, which is
 * what makes check (1) reusable against the probe as well as the archive.
 *
 * The upstream quitLast() ends in exit(1) -- its normal exit path -- so the
 * result is printed from an atexit handler, exactly like the host runner.
 */
#include "noiz2sa_port_testhooks.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* PAD_* masks, mirrored from the engine's screen.h. */
#define PROBE_PAD_BUTTON1 16
#define PROBE_PAD_LEFT 4
#define PROBE_PAD_RIGHT 8

#define PROBE_FRAMES 1800
#define PROBE_PRESS_START_FRAME 60
#define PROBE_PLAY_HOLD_FIRST 100
#define PROBE_PLAY_HOLD_LAST 1700

/* Same scripted scenario as noiz2sa_host_main.c: start stage 1, then hold
   fire with short movement bursts so enemies, shots, BulletML barrage state
   and the score chain all take part in the rendered frame. */
static void replay_input_hook(int frame, int *pad, int *buttons)
{
    if (frame == PROBE_PRESS_START_FRAME) {
        *buttons = PROBE_PAD_BUTTON1;
        return;
    }
    if (frame < PROBE_PLAY_HOLD_FIRST || frame > PROBE_PLAY_HOLD_LAST) {
        return;
    }
    *buttons = PROBE_PAD_BUTTON1;
    switch ((frame / 90) % 4) {
    case 1:
        *pad = PROBE_PAD_LEFT;
        break;
    case 3:
        *pad = PROBE_PAD_RIGHT;
        break;
    default:
        break;
    }
}

/* FNV-1a 64 over the presented RGB565 framebuffer. Same constants and
   iteration order as noiz2sa_port_print_frame_hash(); copied rather than
   shared, because the host digest lives in a host-only translation unit and
   the point of this target is that it does not link one. */
static void probe_atexit(void)
{
    compat_framebuffer_t *fb = compat_framebuffer();
    uint64_t hash = 0xcbf29ce484222325ULL;
    unsigned nonZero = 0;
    unsigned x, y;

    for (y = 0; y < fb->height; y++) {
        const compat_pixel_t *row = fb->pixels + (size_t)y * fb->stride;
        for (x = 0; x < fb->width; x++) {
            hash ^= (uint32_t)row[x];
            hash *= 0x100000001b3ULL;
            if (row[x] != 0) {
                nonZero++;
            }
        }
    }
    printf("NOIZ2SA TARGET PROBE FRAME HASH %016llx nonzero=%u\n",
           (unsigned long long)hash, nonZero);
    printf("NOIZ2SA TARGET PROBE: PASS frames=%d\n", PROBE_FRAMES);
}

int main(int argc, char *argv[])
{
    noiz2sa_port_set_deterministic(1);
    noiz2sa_port_set_frame_limit(PROBE_FRAMES);
    noiz2sa_port_set_input_hook(replay_input_hook);
    atexit(probe_atexit);

    noiz2sa_main(argc, argv);
    return 0;
}
