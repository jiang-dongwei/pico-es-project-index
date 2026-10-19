/* SDL3/SDL_mutex.h -- the Pico port's minimal stand-in for the real header.
 *
 * WHY THIS EXISTS
 * ---------------
 * Upstream reaches SDL through *angle-bracket* includes, even from its own
 * platform-dispatch headers:
 *
 *     src/backend/threading/be_mutex_sdl.h    ->  #include <SDL3/SDL_mutex.h>
 *     src/backend/threading/be_atomic_sdl.h   ->  #include <SDL3/SDL_atomic.h>
 *     src/be_st_sdl_private.h                 ->  #include <SDL3/SDL.h>
 *
 * Quoted includes with an explicit relative path (be_audio_mixer.h does
 * #include "../threading/be_mutex.h") cannot be shadowed from the command
 * line -- but these can. Putting this directory first with -I therefore
 * redirects every SDL include in the tree to the port, and upstream stays
 * byte-identical: verified with `git -C _deps/reflectionhle-upstream status
 * --short` being empty at tag release-20260606.
 *
 * WHY A MUTEX SHIM IS MANDATORY, NOT OPTIONAL
 * -------------------------------------------
 * REFKEEN_CONFIG_THREADS is off for this port, but that does not remove the
 * type dependency: src/backend/audio/be_audio_mixer.h embeds BE_ST_Mutex_T in
 * its own struct unconditionally. With threads off, six TUs (be_audio_digi.c,
 * be_audio_main_thread.c, be_audio_mixer.c, be_audio_opl.c, be_audio_pcspkr.c,
 * be_timing.c) fail to compile. Measured, see platform-boundary.md.
 *
 * Semantics: the port is single-threaded. Lock/Unlock always succeed; the
 * mutex carries state only so that a mistaken double-init is detectable in a
 * debug build.
 */

#ifndef REFKEEN_SDL3_SHIM_SDL_MUTEX_H
#define REFKEEN_SDL3_SHIM_SDL_MUTEX_H

#include <stdbool.h>

/* Opaque, exactly as in the real SDL3 (seen only through pointers). */
typedef struct SDL_Mutex SDL_Mutex;

/* The real SDL3 signatures. Note SDL_LockMutex/SDL_UnlockMutex return bool in
 * SDL3 (they were void in SDL2); upstream discards the result but declares its
 * wrappers as void, so matching the real signature keeps the shim honest. */
SDL_Mutex *SDL_CreateMutex(void);
void SDL_DestroyMutex(SDL_Mutex *mutex);
bool SDL_LockMutex(SDL_Mutex *mutex);
bool SDL_UnlockMutex(SDL_Mutex *mutex);
bool SDL_TryLockMutex(SDL_Mutex *mutex);

#endif /* REFKEEN_SDL3_SHIM_SDL_MUTEX_H */
