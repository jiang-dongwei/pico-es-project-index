/*
 * S17-B headless host replay runner for the pinned noiz2sa engine.
 *
 * Runs noiz2sa_main() for a fixed number of engine frames in deterministic
 * mode with a scripted input scenario (title -> stage select -> in-game
 * with held left+fire -> release -> exit). The SDL shim advances the tick
 * source by the engine's 16 ms interval per call, pinning the rand() seed
 * chain, so the whole run is replay-identical.
 *
 * The upstream quitLast() ends in exit(1) -- its normal exit path -- so
 * success is verified from an atexit handler: the versioned preference file
 * must exist and the state digest line must be printed.
 *
 * Usage: noiz2sa_host_smoke.exe [data_root]
 */
#include "noiz2sa_port_testhooks.h"
#include "noiz2sa.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#endif

/* PAD_* masks from screen.h via the engine's own definitions. */
#define PORT_PAD_BUTTON1 16
#define PORT_PAD_LEFT 4
#define PORT_PAD_RIGHT 8

#define SMOKE_FRAMES 1800
#define PRESS_START_FRAME 60
#define PLAY_HOLD_FIRST 100
#define PLAY_HOLD_LAST 1700

/* Optional PNG dump target, set from argv[2]. */
static const char *g_dumpPath;

/* Scripted scenario: start stage 1, then keep firing with short movement
   bursts so enemies, shots, BulletML barrage state and the score chain all
   take part in the digest. */
static void replay_input_hook(int frame, int *pad, int *buttons)
{
    if (frame == PRESS_START_FRAME) {
        *buttons = PORT_PAD_BUTTON1;
        return;
    }
    if (frame < PLAY_HOLD_FIRST || frame > PLAY_HOLD_LAST) {
        return;
    }
    *buttons = PORT_PAD_BUTTON1;
    switch ((frame / 90) % 4) {
    case 1:
        *pad = PORT_PAD_LEFT;
        break;
    case 3:
        *pad = PORT_PAD_RIGHT;
        break;
    default:
        break;
    }
}

static void verify_atexit(void)
{
    FILE *pref = fopen("noiz2sa.prf", "rb");
    if (pref == NULL) {
        fprintf(stderr, "NOIZ2SA HOST SMOKE: preference file missing\n");
        return;
    }
    fclose(pref);
    noiz2sa_port_print_state_digest();
    noiz2sa_port_print_frame_hash();
    if (g_dumpPath != NULL) {
        if (noiz2sa_port_dump_frame_png(g_dumpPath) == 0) {
            printf("NOIZ2SA FRAME DUMP: %s\n", g_dumpPath);
        } else {
            fprintf(stderr, "NOIZ2SA FRAME DUMP failed: %s\n", g_dumpPath);
        }
    }
    printf("NOIZ2SA HOST SMOKE: PASS frames=%d\n", SMOKE_FRAMES);
}

int main(int argc, char *argv[])
{
    if (argc > 1) {
#ifdef _WIN32
        if (_chdir(argv[1]) != 0) {
#else
        if (chdir(argv[1]) != 0) {
#endif
            fprintf(stderr, "noiz2sa_host_smoke: cannot chdir to %s: %s\n",
                    argv[1], strerror(errno));
            return 2;
        }
    }

    noiz2sa_port_set_deterministic(1);
    noiz2sa_port_set_frame_limit(SMOKE_FRAMES);
    noiz2sa_port_set_input_hook(replay_input_hook);
    if (argc > 2) {
        g_dumpPath = argv[2];
    }
    atexit(verify_atexit);

    noiz2sa_main(1, argv);
    return 0;
}
