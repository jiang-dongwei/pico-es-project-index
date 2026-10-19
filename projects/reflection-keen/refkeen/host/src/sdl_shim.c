/* sdl_shim.c -- linked half of the Pico port's minimal SDL3 stand-in.
 *
 * Pairs with refkeen/host/shim/SDL3/{SDL.h,SDL_mutex.h,SDL_atomic.h}.
 * The atomic API is fully static inline in its header, so nothing for it here.
 *
 * TWO TARGETS, ONE SOURCE
 * -----------------------
 * On the Pico target the timing primitives go to pico-sdk; on a host build
 * (used for compile/measure passes) they go to POSIX.
 *
 * The switch is REFKEEN_SDL_SHIM_HOST, not REFKEEN_PLATFORM_PICO, and that
 * separation is deliberate: Test-ShimSurface.ps1 compiles the engine TUs with
 * the *real* target configuration (REFKEEN_PLATFORM_PICO defined, so the
 * desktop feature block stays off) while swapping only the shim's own platform
 * implementation for the host's. One flag per concern.
 *
 * The default is the target path, so a host build that forgets the flag fails
 * loudly on a missing pico/time.h rather than silently using host timing.
 */

/* Must precede any system header: enables clock_gettime/nanosleep on the
 * MinGW-w64 host path, which otherwise only declares the legacy time.h API. */
#if defined(REFKEEN_SDL_SHIM_HOST) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdbool.h>

#include <SDL3/SDL_mutex.h>

/* ===========================================================================
 * Mutexes
 *
 * Single-threaded target, so a mutex never blocks: Lock/Unlock always succeed
 * and carry no state. What the implementation *does* preserve is the
 * Create/Destroy lifecycle, because callers treat a mutex as an owned
 * resource: Create must hand back a distinct pointer each time (so a
 * double-Create bug is not silently aliased) and Destroy must release the slot.
 *
 * A fixed pool is used rather than malloc: this must never be the reason a
 * heap is linked in on a 520 KiB-SRAM target, and running out of slots is a
 * programming error loud enough to return NULL for.
 * =========================================================================== */

#define SDL_SHIM_MUTEX_SLOTS 8

struct SDL_Mutex {
    unsigned inUse;
};

static struct SDL_Mutex sdlShimMutexes[SDL_SHIM_MUTEX_SLOTS];

SDL_Mutex *SDL_CreateMutex(void)
{
    for (unsigned i = 0; i < SDL_SHIM_MUTEX_SLOTS; ++i) {
        if (!sdlShimMutexes[i].inUse) {
            sdlShimMutexes[i].inUse = 1;
            return &sdlShimMutexes[i];
        }
    }
    return NULL;
}

void SDL_DestroyMutex(SDL_Mutex *mutex)
{
    if (mutex) {
        mutex->inUse = 0;
    }
}

bool SDL_LockMutex(SDL_Mutex *mutex)
{
    /* A NULL mutex is what an unchecked failed Create looks like; returning
     * true would hide it, so fail instead. */
    return mutex != NULL;
}

bool SDL_UnlockMutex(SDL_Mutex *mutex)
{
    return mutex != NULL;
}

bool SDL_TryLockMutex(SDL_Mutex *mutex)
{
    /* Never contended: if it exists at all, it is already ours. */
    return mutex != NULL;
}

/* ===========================================================================
 * Timing
 *
 * SDL_GetTicks contract: milliseconds since SDL initialisation, as uint32.
 * The port measures from reset instead, which is the same thing here (there is
 * no SDL_Init), and lets the 32-bit value wrap like SDL's does.
 *
 * Wrapping matters: a naive difference of two wrapped values is still correct
 * in unsigned arithmetic, which is how upstream uses it. Keep the unsigned
 * subtraction on the caller's side by returning uint32_t, not int64_t.
 * =========================================================================== */

/* The target path is the default; see the header comment. The host pass
 * defines REFKEEN_SDL_SHIM_HOST to swap in the POSIX implementation. */
#if !defined(REFKEEN_SDL_SHIM_HOST)

#include "pico/time.h"   /* time_us_64(), sleep_ms() */

uint64_t SDL_GetTicksNS(void)
{
    /* time_us_64 is monotonic since boot; 64-bit microseconds cannot overflow
     * into the 64-bit nanoseconds result for ~584 years. */
    return time_us_64() * 1000ull;
}

void SDL_Delay(uint32_t ms)
{
    sleep_ms(ms);
}

#else /* host-side compile/measure pass */

#include <time.h>

uint64_t SDL_GetTicksNS(void)
{
    struct timespec ts;
    /* CLOCK_MONOTONIC, not the wall clock: upstream uses deltas, and a wall
     * clock step would show up as a bogus frame-time spike. */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

void SDL_Delay(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    nanosleep(&ts, NULL);
}

#endif /* !REFKEEN_SDL_SHIM_HOST */

uint32_t SDL_GetTicks(void)
{
    return (uint32_t)(SDL_GetTicksNS() / 1000000ull);
}
