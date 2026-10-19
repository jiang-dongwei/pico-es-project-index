/*
 * RetroPicoES v2.1 engine/shim boundary for the pinned noiz2sa engine.
 *
 * The engine keeps its upstream sources; every platform adaptation is owned
 * by this port layer and routed through compat v1 (compat.h) or these hooks.
 *
 * This header carries the *engine <-> shim* contract only. Host deterministic
 * test scaffolding lives in noiz2sa_port_testhooks.h; target-only adaptation
 * lives in noiz2sa_port_target.h. Keeping them apart matters because the host
 * runner must not see target policy, and a target image must not pull in host
 * replay hooks.
 */
#ifndef NOIZ2SA_PORT_H
#define NOIZ2SA_PORT_H

#include "SDL.h"
#include "compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shim key state array size, indexed by SDL scancode. */
#define NOIZ2SA_PORT_KEYSTATE_SIZE 256

/* Direct access to the shim keyboard state (SDL scancode indexed). */
Uint8 *noiz2sa_port_keyboard_state(void);

/* Engine entry point, renamed from main() by the recorded port patch. */
int noiz2sa_main(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* NOIZ2SA_PORT_H */
