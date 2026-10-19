/* SDL3/SDL.h -- the Pico port's minimal stand-in for the real umbrella header.
 *
 * SCOPE: this is not an SDL implementation. It covers exactly the SDL surface
 * that the *non-platform* translation units touch, so that the port can
 * compile the engine and the platform-independent backend layers without
 * pulling in libSDL or the desktop build. The seven platform TUs the port
 * replaces (video, input, audio, startup, cfg, app paths) are rewritten
 * against the hardware directly and never see this header.
 *
 * How the surface was determined: compile every TU upstream's CMake would
 * build, except the seven platform TUs and the launcher, with this directory
 * first on the include path, and see what the compiler still wants. The
 * measured list lives in .emv2/logs/s20-shim-surface.txt; anything added here
 * should be traceable to a diagnostic in that log, not to speculation.
 *
 * NOT PROVIDED, DELIBERATELY:
 *   SDL_main.h      -- actual_main.c is replaced by the port's own entry point
 *   SDL_render.h    -- only be_cfg_sdl.c (a replaced platform TU) includes it
 *   SDL_gamepad.h   -- only be_input_sdl.h (a replaced platform TU) includes it
 *   SDL_touch.h     -- only be_video_sdl.h (a replaced platform TU) includes it
 *   SDL_system.h    -- only be_filesystem_app_paths.c (a replaced TU) includes it
 *
 * INCLUDE ORDER: upstream requires refkeen_config.h to be seen before anything
 * that depends on its macros. It is brought in here because SDL3/SDL.h is the
 * earliest header reached by every TU that uses this shim (they all go through
 * be_st_sdl_private.h or be_mutex_sdl.h).
 */

#ifndef REFKEEN_SDL3_SHIM_SDL_H
#define REFKEEN_SDL3_SHIM_SDL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_atomic.h>
/* Real SDL3's umbrella reaches the scancode enum through SDL_events.h, and
 * upstream relies on that: be_events_sdl.c declares
 * `sdlKeyMappings[SDL_SCANCODE_COUNT]` with nothing but be_st_sdl_private.h
 * (-> <SDL3/SDL.h>) in front of it. The shim has to reproduce the transitive
 * include, or the port's replacement for that TU cannot be shaped like it.
 * Found by compiling refkeen/platform/retropicoes/be_events_port.c, which is
 * the eighth platform TU and so was outside the surface measurement that
 * produced this file (see .emv2/logs/s20-shim-surface.txt). */
#include <SDL3/SDL_scancode.h>

/* ---------------------------------------------------------------------------
 * Scheduling primitives
 *
 * Upstream's be_st_sdl_private.h maps the BEL_ST_* seam onto these:
 *     BEL_ST_SleepMS             -> SDL_Delay
 *     BEL_ST_CPUPauseInstruction -> SDL_CPUPauseInstruction
 *     BEL_ST_GetTicksMS()        -> SDL_GetTicks
 *     BEL_ST_GetTicksNS()        -> SDL_GetTicksNS
 * Because that header reaches SDL through angle brackets, supplying these four
 * names is enough to satisfy the seam -- be_st_sdl_private.h itself stays
 * untouched. Implementations live in refkeen/host/src/sdl_shim.c.
 * ------------------------------------------------------------------------ */

void SDL_Delay(uint32_t ms);
uint32_t SDL_GetTicks(void);
uint64_t SDL_GetTicksNS(void);
const char *SDL_GetRenderDriver(int index);

/* The real SDL3 defines this as an architecture-specific pause hint. Keeping
 * it a function (not a macro) is fine: upstream expands
 * BEL_ST_CPUPauseInstruction to the identifier SDL_CPUPauseInstruction and
 * always calls it with parentheses. */
static inline void SDL_CPUPauseInstruction(void)
{
#if defined(__arm__) || defined(__thumb__) || defined(__aarch64__)
    __asm__ __volatile__("yield");
#elif defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("pause");
#else
    /* No architectural hint: emit a compiler barrier so the intent (do not
     * hoist memory operations past this point) survives without code. */
    __asm__ __volatile__("" ::: "memory");
#endif
}

#endif /* REFKEEN_SDL3_SHIM_SDL_H */
