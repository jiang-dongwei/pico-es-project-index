/* SDL3/SDL_gamepad.h -- the Pico port's minimal stand-in for the real header.
 *
 * Needed because a *non-platform* TU pulls it in: be_input_controller_mappings.c
 * includes be_input_sdl.h, which declares the joystick/controller arrays the
 * mapping table is built from. Those declarations use SDL handle types.
 *
 * The handles stay opaque: the port's input layer never dereferences them, it
 * only stores and compares them. SDL_JoystickID is a real integer because it is
 * compared by value.
 *
 * Sizes match SDL3: SDL_JoystickID is Uint32.
 */

#ifndef REFKEEN_SDL3_SHIM_SDL_GAMEPAD_H
#define REFKEEN_SDL3_SHIM_SDL_GAMEPAD_H

#include <stdint.h>

typedef struct SDL_Joystick SDL_Joystick;
typedef struct SDL_Gamepad SDL_Gamepad;
typedef uint32_t SDL_JoystickID;

#endif /* REFKEEN_SDL3_SHIM_SDL_GAMEPAD_H */
