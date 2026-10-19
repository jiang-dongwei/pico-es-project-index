/* refkeen_config.h -- the Pico port's own configuration header.
 *
 * Upstream generates this file with CMake's configure_file() from
 * refkeen_config.h.in. The port does not use that path: the template's
 * platform macros are auto-detected from __unix__ / __APPLE__ / _WIN32 /
 * EMSCRIPTEN, and adding REFKEEN_PLATFORM_PICO there would mean editing an
 * upstream file. Instead the port ships this header and puts its directory
 * first on the include path, so every upstream include of "refkeen_config.h"
 * resolves here. Upstream stays byte-identical.
 *
 * Upstream requires this header to be included early (see the comment at the
 * top of refkeen_config.h.in), because the macros below steer the rest of the
 * translation unit.
 *
 * TWO THINGS THIS FILE TAUGHT ME THE HARD WAY
 * ------------------------------------------
 * 1. Anything that is a property of the *port* rather than of the Pico branch
 *    must sit outside #ifdef REFKEEN_PLATFORM_PICO. Putting
 *    REFKEEN_RESAMPLER_NONE inside it left the host-side compile pass with
 *    neither resampler macro defined, and be_audio_resampling.c failed with a
 *    "not explained by the platform seam" error that pointed at the audio
 *    code rather than at this file.
 * 2. The desktop feature block is gated upstream on (!ANDROID && !EMSCRIPTEN).
 *    A host compile therefore enables all of it unless the host pass also
 *    defines REFKEEN_PLATFORM_PICO; see Test-ShimSurface.ps1, which does. The
 *    condition below adds the Pico exclusion so that the target build is right
 *    even without that flag.
 */

#ifndef REFKEEN_CONFIG_H
#define REFKEEN_CONFIG_H

/* ---------------------------------------------------------------------------
 * Platform detection
 *
 * Follows the upstream idiom of detecting from the compiler's predefined
 * macros rather than from a build-system switch, so that a host-side
 * compile/measure pass (no pico-sdk present) naturally takes the non-Pico
 * branch without anyone having to remember a flag.
 * ------------------------------------------------------------------------ */

#if (defined PICO_BOARD) || (defined PICO_ON_DEVICE) || (defined PICO_RP2350) || (defined PICO_RP2040)
#define REFKEEN_PLATFORM_PICO
#endif

/* ---------------------------------------------------------------------------
 * Properties of the port as a whole (not of the Pico branch)
 * ------------------------------------------------------------------------ */

/* RP2350B is little-endian, and every host used for the compile/measure pass
 * is too. Unconditional on purpose. */
#define REFKEEN_ARCH_LITTLE_ENDIAN

/* SpeexDSP is not available on the target and its resampler is not wanted:
 * the port targets the emulated OPL / PC-speaker path, and CPU headroom is the
 * binding constraint, not resampler quality. Unconditional on purpose --
 * be_audio_resampling.c does not compile when neither REFKEEN_RESAMPLER_*
 * macro is defined, and "no resampler" is a port decision, not a Pico one. */
#define REFKEEN_RESAMPLER_NONE "1"

/* Single game, no launcher, no unified multi-game executable. */
/* REFKEEN_ENABLE_LAUNCHER intentionally undefined */
/* REFKEEN_UNIFIED_BUILD intentionally undefined */

/* Threads: COUNTER-INTUITIVE, and measured rather than assumed.
 *
 * The intuitive move for a single-core target is to leave
 * REFKEEN_CONFIG_THREADS undefined, exactly as upstream does for Emscripten.
 * That is wrong here, and S20-B measured why: upstream's dispatch headers are
 * guarded *entirely* by this macro --
 *
 *     src/backend/threading/be_mutex.h   ->  #ifdef REFKEEN_CONFIG_THREADS
 *                                            #include "be_mutex_sdl.h"
 *                                            #endif
 *
 * so leaving it undefined does not merely stop thread *creation*, it deletes
 * BE_ST_Mutex_T and BE_ST_AtomicInt_T outright. be_audio_mixer.h embeds
 * BE_ST_Mutex_T in its own struct with no guard at all, so seven TUs fail to
 * compile with `unknown type name 'BE_ST_Mutex_T'` -- among them the audio path
 * the port depends on:
 *
 *     be_audio_digi.c be_audio_init.c be_audio_main_thread.c
 *     be_audio_mixer.c be_audio_opl.c be_audio_pcspkr.c be_timing.c
 *
 * The port therefore DEFINES the macro and expresses "single-threaded" through
 * what the shim's mutex/atomic implementations DO (never block, always
 * succeed), not through this switch. Excluding a shim by #ifdef does not work
 * here: the shim is behind the same guard as the type.
 *
 * S20-B status: defining it is the measured-correct choice for compilation.
 * Whether upstream then takes a thread-creating path at runtime, and what the
 * shim must supply if so, is the first item of S20-C. */
#define REFKEEN_CONFIG_THREADS

/* ---------------------------------------------------------------------------
 * Desktop feature block
 *
 * Upstream gates these on (!ANDROID && !EMSCRIPTEN). Reproduce that condition
 * while also excluding the Pico target, without touching the upstream
 * template. Keep the expression shape identical to upstream's so that a diff
 * against refkeen_config.h.in stays readable.
 * ------------------------------------------------------------------------ */

#if (!defined REFKEEN_PLATFORM_ANDROID) && (!defined REFKEEN_PLATFORM_EMSCRIPTEN) && (!defined REFKEEN_PLATFORM_PICO)
#define REFKEEN_CONFIG_USER_FULLSCREEN_TOGGLE
#define REFKEEN_CONFIG_USER_FULLSCREEN_RES_SETTING
#define REFKEEN_CONFIG_LAUNCHER_WINDOWTYPE_MENUITEM
#define REFKEEN_CONFIG_CHECK_FOR_STEAM_INSTALLATION
#define REFKEEN_CONFIG_ENABLE_CMDLINE
#endif

/* Touch input: upstream enables it on everything except macOS. A 320x240 DVI
 * panel has no touch controller; the port's input layer reports keys only. */
#if (!defined REFKEEN_PLATFORM_MACOS) && (!defined REFKEEN_PLATFORM_PICO)
#define REFKEEN_CONFIG_ENABLE_TOUCHINPUT
#endif

/* LPT passthrough needs a real parallel port. */
#if (((defined REFKEEN_PLATFORM_LINUX) && (!defined REFKEEN_PLATFORM_ANDROID)) || (defined REFKEEN_PLATFORM_WINDOWS)) && (!defined REFKEEN_PLATFORM_PICO)
#define REFKEEN_CONFIG_LPT_PASSTHROUGH
#endif

/* Sanity check, kept from upstream. */
#if (!defined REFKEEN_CONFIG_THREADS) && (defined REFKEEN_CONFIG_EVENTS_CALLBACK)
#error "REFKEEN sanity check failed - REFKEEN_CONFIG_EVENTS_CALLBACK is defined, but REFKEEN_CONFIG_THREADS is not!"
#endif

#endif /* REFKEEN_CONFIG_H */
