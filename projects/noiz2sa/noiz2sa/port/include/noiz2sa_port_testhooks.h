/*
 * Host deterministic-replay scaffolding for the noiz2sa port.
 *
 * Everything declared here exists so the pinned engine can be replayed
 * headlessly and compared bit-for-bit (S17-B/C evidence). These symbols are
 * compiled into the host runner only; a target build links the same
 * noiz2sa_game archive but never calls any of them.
 */
#ifndef NOIZ2SA_PORT_TESTHOOKS_H
#define NOIZ2SA_PORT_TESTHOOKS_H

#include "noiz2sa_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Deterministic mode: the shim advances its tick source by the engine's 16 ms
   frame interval per call instead of reading the wall clock, so the rand()
   seed chain is pinned. */
void noiz2sa_port_set_deterministic(int enabled);
int noiz2sa_port_deterministic(void);

/* Headless smoke support: after this many engine loop iterations the shim
   presses ESC so noiz2sa_main() exits cleanly. Negative disables the limit. */
void noiz2sa_port_set_frame_limit(int frames);

/*
 * Per-engine-frame input hook for scripted/replay tests. The shim counts
 * engine loop iterations (one SDL_PollEvent per frame); before the frame the
 * screen adapter samples pad/buttons, the hook (if set) may overwrite the
 * latched values for the current frame. Returning pad/button bitfields
 * follows the engine's PAD_* masks in screen.h.
 */
typedef void (*noiz2sa_port_input_hook_t)(int frame, int *pad, int *buttons);
void noiz2sa_port_set_input_hook(noiz2sa_port_input_hook_t hook);
noiz2sa_port_input_hook_t noiz2sa_port_input_hook_target(void);
int noiz2sa_port_frame(void);

/* Host-side deterministic state digest (noiz2sa_state_digest.cpp). */
void noiz2sa_port_print_state_digest(void);

/* Host-side digest of the presented RGB565 framebuffer, proving the video
   pipeline output is replay-identical (noiz2sa_state_digest.cpp). */
void noiz2sa_port_print_frame_hash(void);

/* Host-only: write the presented framebuffer as an uncompressed RGB888 PNG
   for visual inspection (noiz2sa_png_dump.c). Not linked into target builds. */
int noiz2sa_port_dump_frame_png(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* NOIZ2SA_PORT_TESTHOOKS_H */
