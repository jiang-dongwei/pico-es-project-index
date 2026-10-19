/*
 * SDL shim implementation for the noiz2sa RetroPicoES port.
 *
 * Maps the small SDL surface referenced by the retained engine units onto
 * compat v1 timers plus port state. In deterministic mode (host tests) the
 * tick source advances by the engine's 16 ms frame interval per call so the
 * game loop executes exactly one move() per iteration with a stable rand()
 * seed chain.
 */
#include "SDL.h"
#include "noiz2sa_port.h"
#include "noiz2sa_port_testhooks.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define NOIZ2SA_PORT_DETERMINISTIC_STEP_MS 16u

static Uint8 g_keyboard_state[NOIZ2SA_PORT_KEYSTATE_SIZE];
static int g_deterministic;
static int g_frame_limit = -1;
static int g_poll_count;
static int g_engine_frame;
static uint32_t g_virtual_ms;
static noiz2sa_port_input_hook_t g_input_hook;

void noiz2sa_port_set_input_hook(noiz2sa_port_input_hook_t hook)
{
    g_input_hook = hook;
}

noiz2sa_port_input_hook_t noiz2sa_port_input_hook_target(void)
{
    return g_input_hook;
}

int noiz2sa_port_frame(void)
{
    return g_engine_frame;
}

void noiz2sa_port_set_deterministic(int enabled)
{
    g_deterministic = enabled ? 1 : 0;
}

int noiz2sa_port_deterministic(void)
{
    return g_deterministic;
}

void noiz2sa_port_set_frame_limit(int frames)
{
    g_frame_limit = frames;
}

Uint8 *noiz2sa_port_keyboard_state(void)
{
    return g_keyboard_state;
}

Uint8 *SDL_GetKeyboardState(int *numkeys)
{
    if (numkeys != NULL) {
        *numkeys = NOIZ2SA_PORT_KEYSTATE_SIZE;
    }
    return g_keyboard_state;
}

int SDL_PollEvent(SDL_Event *event)
{
    if (event != NULL) {
        event->type = 0;
    }
    g_engine_frame++;
    if (g_frame_limit > 0 && g_engine_frame >= g_frame_limit) {
        g_keyboard_state[SDL_SCANCODE_ESCAPE] = SDL_PRESSED;
    }
    return 0;
}

Uint32 SDL_GetTicks(void)
{
    if (g_deterministic) {
        g_virtual_ms += NOIZ2SA_PORT_DETERMINISTIC_STEP_MS;
        return g_virtual_ms;
    }
    return (Uint32)compat_ticks_ms();
}

void SDL_Delay(Uint32 ms)
{
    if (!g_deterministic) {
        compat_delay_ms(ms);
    }
}

void SDL_Quit(void)
{
    /* compat_shutdown() is owned by the port runtime, not the engine. */
}

/* Portable getw/putw over FILE*. The engine only uses them for the
   versioned preference file; the on-disk layout stays little-endian raw
   ints, which matches both the x86/Windows origin and RP2350B.

   Defined under port-unique names; SDL.h redirects the engine's call sites
   with macros so the port never publishes the bare libc symbols. */
int noiz2sa_port_getw(FILE *stream)
{
    int value = 0;
    if (stream == NULL ||
        fread(&value, sizeof(value), 1, stream) != 1) {
        return EOF;
    }
    return value;
}

int noiz2sa_port_putw(int value, FILE *stream)
{
    if (stream == NULL ||
        fwrite(&value, sizeof(value), 1, stream) != 1) {
        return EOF;
    }
    return value;
}
