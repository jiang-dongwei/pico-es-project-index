/*
 * SDL shim header for the noiz2sa RetroPicoES port.
 *
 * The pinned noiz2sa engine includes "SDL.h" from every translation unit, but
 * the SDL API surface actually referenced outside the two replaced files
 * (screen.c and soundmanager.c) is tiny. This shim is found through the
 * include path before any real SDL, so the upstream sources stay unmodified.
 *
 * Only the symbols referenced by the retained engine units are declared:
 * Uint8/Uint16/Uint32, SDL_Color, SDL_Event, SDL_Joystick, SDL_PRESSED,
 * SDL_RELEASED, SDL_QUIT, SDL_GetTicks, SDL_Delay, SDL_Quit, SDL_PollEvent,
 * SDL_GetKeyboardState, SDL_SCANCODE_ESCAPE and SDL_SCANCODE_P.
 *
 * The shim also declares getw/putw (used by attractmanager.c preferences)
 * because they are nonstandard and not uniformly declared by host libcs; the
 * port implements both portably over FILE*.
 */
#ifndef NOIZ2SA_PORT_SDL_SHIM_H
#define NOIZ2SA_PORT_SDL_SHIM_H

#include <stdio.h>
#include <stdint.h>

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;

#define SDL_PRESSED  1
#define SDL_RELEASED 0

/* Application quit event type; the port only polls for quit. */
#define SDL_QUIT 0x100

/* Scancodes used by the engine outside screen.c. */
#define SDL_SCANCODE_ESCAPE 41
#define SDL_SCANCODE_P      19

typedef struct {
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 unused;
} SDL_Color;

typedef struct {
    int type;
} SDL_Event;

/* The engine stores the joystick handle but never dereferences it outside
   the replaced screen.c. */
typedef struct SDL_Joystick SDL_Joystick;

Uint8 *SDL_GetKeyboardState(int *numkeys);
int SDL_PollEvent(SDL_Event *event);
Uint32 SDL_GetTicks(void);
void SDL_Delay(Uint32 ms);
void SDL_Quit(void);

/* Portable implementations provided by the port (sdl_shim.c).
 *
 * Both are published under port-unique names rather than the bare libc
 * getw/putw. Those two are nonstandard, so a libc is free to declare or
 * define them (newlib on RP2350B may), and defining the bare names would
 * then depend on linker symbol interposition to override the C library.
 * The macros below rewrite the engine's call sites in attractmanager.c to
 * the port functions instead.
 *
 * The macros cannot contaminate libc declarations: <stdio.h> is included
 * above, before these macros exist, and its include guard stops any later
 * translation unit from re-parsing those declarations. */
int noiz2sa_port_getw(FILE *stream);
int noiz2sa_port_putw(int value, FILE *stream);
#define getw noiz2sa_port_getw
#define putw noiz2sa_port_putw

#endif /* NOIZ2SA_PORT_SDL_SHIM_H */
