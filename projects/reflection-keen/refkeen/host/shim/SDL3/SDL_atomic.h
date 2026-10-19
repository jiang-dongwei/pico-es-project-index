/* SDL3/SDL_atomic.h -- the Pico port's minimal stand-in for the real header.
 *
 * See SDL_mutex.h for why angle-bracket includes can be redirected this way
 * while upstream stays untouched.
 *
 * The functions are static inline over GCC/Clang __atomic_* builtins rather
 * than a linked implementation: every TU that needs them gets the same
 * lock-free code, which is what the real SDL3 does internally too. On a single
 * Cortex-M33 core these compile to plain loads/stores plus, for the read-
 * modify-write forms, a single-instruction-strength sequence -- no libatomic.
 *
 * RELATIONSHIP TO REFKEEN_CONFIG_THREADS
 * --------------------------------------
 * src/backend/timing/be_timing.c includes this through be_atomic.h even with
 * threads off, and its BE_ST_AtomicInt_* wrappers must still link and behave
 * correctly when called from the one thread that exists. Hence a real
 * implementation, not no-op stubs.
 *
 * RETURN-VALUE CONTRACT (checked against how upstream uses them)
 * -------------------------------------------------------------
 * Upstream's be_atomic_sdl.h returns the result of each call as int, so all
 * four must be value-returning, not void:
 *
 *   BE_ST_AtomicInt_Set(a, n)     -> SDL_SetAtomicInt       (int, previous value)
 *   BE_ST_AtomicInt_Get(a)        -> SDL_GetAtomicInt       (int, current value)
 *   BE_ST_AtomicInt_PostAdd(a, n) -> SDL_AddAtomicInt       (int, previous value)
 *   BE_ST_AtomicInt_PostInc(a)    -> SDL_AddAtomicInt(a, 1)
 *
 * SDL3 documents Set/Add as returning the *previous* value, which is what
 * __atomic_exchange_n / __atomic_fetch_add give. That matters even with one
 * thread: the return value is observable, so a wrong choice here would be a
 * silent behaviour change rather than a compile error.
 */

#ifndef REFKEEN_SDL3_SHIM_SDL_ATOMIC_H
#define REFKEEN_SDL3_SHIM_SDL_ATOMIC_H

#include <stdbool.h>

typedef struct SDL_AtomicInt { int value; } SDL_AtomicInt;

static inline int SDL_SetAtomicInt(SDL_AtomicInt *a, int v)
{
    /* exchange, not store: must return the previous value */
    return __atomic_exchange_n(&a->value, v, __ATOMIC_SEQ_CST);
}

static inline int SDL_GetAtomicInt(SDL_AtomicInt *a)
{
    return __atomic_load_n(&a->value, __ATOMIC_SEQ_CST);
}

static inline int SDL_AddAtomicInt(SDL_AtomicInt *a, int v)
{
    /* fetch_add: must return the previous value */
    return __atomic_fetch_add(&a->value, v, __ATOMIC_SEQ_CST);
}

static inline bool SDL_CompareAndSwapAtomicInt(SDL_AtomicInt *a, int oldval, int newval)
{
    return __atomic_compare_exchange_n(&a->value, &oldval, newval, false,
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

#endif /* REFKEEN_SDL3_SHIM_SDL_ATOMIC_H */
