/* SDL3/SDL_touch.h -- the Pico port's minimal stand-in for the real header.
 *
 * Needed because be_video_sdl.h pulls it in, and be_video_sdl.h is reached by
 * non-platform TUs (be_input_ui.c, be_video_ui.c). Upstream's own comment there
 * says all that may be assumed of these values is that they compare equal or
 * different -- which is exactly what an integer gives, so no structure is
 * needed.
 *
 * Sizes match SDL3: both are Uint64.
 */

#ifndef REFKEEN_SDL3_SHIM_SDL_TOUCH_H
#define REFKEEN_SDL3_SHIM_SDL_TOUCH_H

#include <stdint.h>

typedef uint64_t SDL_TouchID;
typedef uint64_t SDL_FingerID;

#endif /* REFKEEN_SDL3_SHIM_SDL_TOUCH_H */
