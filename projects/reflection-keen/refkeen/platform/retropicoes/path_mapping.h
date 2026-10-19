/*
 * Port-side path mapping for the ReflectionHLE ("refkeen") Keen Dreams
 * backend on Pico.
 *
 * This module is the one place that answers two questions:
 *
 *   1. Where on the medium does each upstream path root live?
 *   2. How does a filename the engine asks for turn into a medium path?
 *
 * It is deliberately free of any file system calls so that it can be tested
 * on the host; the file system side (dir enumeration, mkdir) sits in the
 * adapter and is a separate step.
 *
 * Upstream facts below are from ReflectionHLE release-20260606,
 * commit 74f07efcc40142a5727caacebec848c48b141eca, and are re-checked on
 * every gate run by refkeen/scripts/Test-PathMappingContract.ps1.
 *
 * ---------------------------------------------------------------------------
 * The four upstream path variables a platform has to supply
 * ---------------------------------------------------------------------------
 *
 *   instPath              BE_GameInstallation_T.instPath
 *                         (be_filesystem_gameinst.h; it is also the `searchdir`
 *                          handed to BEL_Cross_ConditionallyAddGameInstallation)
 *   writableFilesPath     BE_GameInstallation_T.writableFilesPath, built by
 *                         upstream itself as
 *                             g_be_appDataPath + "/" + details->writableFilesDir
 *                         (be_filesystem_gameinst.c:163-164)
 *   g_be_appNewCfgPath    set by BE_Cross_PrepareAppPaths
 *                         (be_filesystem_app_paths.c; port-owned on Pico)
 *   g_be_modPath          set by BE_Cross_SetModPath (be_filesystem_mods.c)
 *
 * All four end up as the `searchdir` argument of
 * BEL_Cross_apply_file_action_in_dir, so all four are part of this contract.
 *
 * Port mapping:
 *
 *   instPath            -> /data/refkeen                        (read-only)
 *   g_be_appDataPath    -> /SAVES/refkeen                       (writable)
 *   g_be_appNewCfgPath  -> /SAVES/refkeen                       (writable, same dir)
 *   writableFilesPath   -> /SAVES/refkeen/<writableFilesDir>    (writable, per version)
 *   g_be_modPath        -> never set; stays ""                  (mods unsupported)
 *
 * Two consequences worth stating explicitly, because they are not obvious:
 *
 *   - g_be_appDataPath and g_be_appNewCfgPath are the *same* directory here.
 *     Upstream splits them (XDG_DATA_HOME vs XDG_CONFIG_HOME, APPDATA vs the
 *     data dir) but Pico has no reason to, and the only observable difference
 *     is that "reflectionhle.cfg" / "gamecontrollerdb.txt" land next to the
 *     per-version save directories instead of beside them. Recorded as a
 *     deliberate port simplification, not an oversight.
 *
 *   - writableFilesPath is *not* the save root: upstream appends the
 *     per-version directory name itself (writableFilesDir, e.g. "kdreamse193").
 *     The port must therefore be able to create /SAVES/refkeen/<version>/,
 *     not just /SAVES/refkeen/.
 *
 * ---------------------------------------------------------------------------
 * Why an empty g_be_modPath is safe
 * ---------------------------------------------------------------------------
 *
 * BE_Cross_open_readonly_for_reading tries writableFilesPath, then modPath,
 * then instPath (be_filesystem_file_ops.c:146-151). With g_be_modPath left as
 * the empty string, the port's BEL_Cross_OpenDir("") returns NULL and the loop
 * simply falls through to instPath. That is the same shape upstream already
 * has for an absent search path, so the port relies on it instead of adding a
 * branch of its own. The adapter therefore *must* fail an empty directory name
 * cleanly; the adapter's own gate has to pin that, because this module cannot
 * (it never calls into the file system).
 */

#ifndef RETROPICOES_PATH_MAPPING_H
#define RETROPICOES_PATH_MAPPING_H

#include <stdbool.h>
#include <stddef.h>

/* --------------------------------------------------------------------------
 * Medium-side bases. No trailing separator: the composer inserts exactly one.
 * -------------------------------------------------------------------------- */

#define RETROPICOES_PATH_GAME_DATA_BASE "/data/refkeen"
#define RETROPICOES_PATH_SAVE_ROOT_BASE "/SAVES/refkeen"

/* --------------------------------------------------------------------------
 * Length bounds.
 *
 * The first two mirror upstream verbatim (be_filesystem_len_bounds.h) and the
 * gate fails if they ever drift apart:
 *
 *   BE_CROSS_PATH_LEN_BOUND         256   full path, including the NUL
 *   BE_CROSS_DOS_FILENAME_LEN_BOUND  13   file name, including the NUL
 * -------------------------------------------------------------------------- */

#define RETROPICOES_PATH_MEDIUM_BOUND   256
#define RETROPICOES_PATH_DOS_NAME_BOUND  13
#define RETROPICOES_PATH_DOS_NAME_MAX   (RETROPICOES_PATH_DOS_NAME_BOUND - 1) /* 12 */

/*
 * ...but the DOS bound only covers part of the surface. Upstream bounds the
 * *8.3 game files* (it copies them into char[13]: be_filesystem_file_ops.c:135,
 * be_filesystem_gameinst.c:86) and never bounds anything else, because the
 * config path reads straight into a 256-byte path (file_ops.c:206-222).
 *
 * Names that really do reach the save root, measured rather than guessed:
 *
 *   SAVEGM0.KDR .. SAVEGM6.KDR   10   id_us.c:252-267, MaxSaveGames = 7
 *   KDREAMS.CFG                  11   id_us.c:4212
 *   CONFIG.KDR                   10   id_us.c:4212 (current_gamever_int < 110)
 *   reflectionhle.cfg            16   be_cfg.c:669
 *   gamecontrollerdb.txt         20   be_startup_sdl.c:85
 *
 * So a single DOS bound would not fit the save root. 63 is a port policy
 * choice: comfortably above every name upstream is known to ask for, and
 * still short enough that base + name can never approach the 256 path bound.
 * Exceeding it is a *port* rejection, i.e. a behaviour deviation -- it is
 * recorded here rather than hidden in the adapter.
 */
#define RETROPICOES_PATH_LONG_NAME_MAX 63

/* --------------------------------------------------------------------------
 * Roles. One per medium-side destination, not one per upstream variable:
 * g_be_appDataPath and g_be_appNewCfgPath share a destination on Pico.
 * -------------------------------------------------------------------------- */

typedef enum {
	RETROPICOES_PATH_ROLE_GAME_DATA = 0, /* instPath; read-only */
	RETROPICOES_PATH_ROLE_SAVE_ROOT = 1, /* writable root for saves and config */
	RETROPICOES_PATH_ROLE_COUNT = 2
} retropicoes_path_role_t;

typedef enum {
	RETROPICOES_PATH_OK = 0,
	RETROPICOES_PATH_ERR_ROLE,            /* role out of range */
	RETROPICOES_PATH_ERR_NAME_EMPTY,      /* name is NULL or "" */
	RETROPICOES_PATH_ERR_NAME_NON_ASCII,  /* a byte outside [32, 126] */
	RETROPICOES_PATH_ERR_NAME_ILLEGAL,    /* '/', '\\', ':' or a Windows-illegal byte */
	RETROPICOES_PATH_ERR_NAME_DOT,        /* "." or ".." */
	RETROPICOES_PATH_ERR_NAME_TRAILING_SPACE,
	RETROPICOES_PATH_ERR_NAME_TOO_LONG,
	RETROPICOES_PATH_ERR_TRUNCATED        /* output buffer too small; nothing written */
} retropicoes_path_result_t;

/* Base path of a role, or NULL if the role is out of range. */
const char *retropicoes_path_role_base(retropicoes_path_role_t role);

/* True when files under this role may be created, rewritten or deleted. */
bool retropicoes_path_role_is_writable(retropicoes_path_role_t role);

/* Longest acceptable single-component name for a role. */
size_t retropicoes_path_role_name_max(retropicoes_path_role_t role);

/*
 * Validate one single-component ASCII file name exactly as the engine would
 * present it, against an explicit maximum length. Returns RETROPICOES_PATH_OK
 * or the first rule the name breaks.
 *
 * CONVENTION, and it is easy to get wrong: all `name_max` parameters in this
 * header count **characters excluding the NUL terminator**, matching
 * RETROPICOES_PATH_DOS_NAME_MAX / RETROPICOES_PATH_LONG_NAME_MAX.
 * RETROPICOES_PATH_MEDIUM_BOUND is the one exception: it is the literal
 * upstream array size and therefore **includes** the NUL, because callers
 * allocate buffers of exactly that size.
 *
 * Note on trailing spaces: upstream strips them in
 * BEL_Cross_CreateTrimmedFilename before the file system layer ever sees a
 * name (be_filesystem_file_ops.c:111-130), but that helper is bypassed by the
 * config path (file_ops.c:206-222). A trailing space here therefore means a
 * caller reached us without going through upstream's trim -- the port rejects
 * it rather than composing a path that the target medium would either refuse
 * or silently mangle.
 */
retropicoes_path_result_t retropicoes_path_check_name(const char *name, size_t name_max);

/*
 * Lower-case canonical on-medium form of a name.
 *
 * Lower case is not an aesthetic choice: upstream creates *new* files with
 * BE_Cross_tolower applied (be_filesystem_file_ops.c:96-103) and finds
 * existing ones by scanning the directory case-insensitively, so lower case
 * is the form that both sides agree on.
 */
retropicoes_path_result_t retropicoes_path_canonical_name(
	const char *name, char *out, size_t out_size);

/* base + "/" + canonical(name), validated against an explicit name bound. */
retropicoes_path_result_t retropicoes_path_compose(
	const char *base, const char *name, size_t name_max,
	char *out, size_t out_size);

/* retropicoes_path_compose with the role's base and name bound. */
retropicoes_path_result_t retropicoes_path_for_role(
	retropicoes_path_role_t role, const char *name, char *out, size_t out_size);

/*
 * /SAVES/refkeen/<version dir>, i.e. exactly what upstream builds as
 * writableFilesPath. `version_dir` is the writableFilesDir of the selected
 * game version (see BE_STR_GAMEVER_* in be_gamestr.h, e.g. "kdreamse193").
 */
retropicoes_path_result_t retropicoes_path_writable_dir(
	const char *version_dir, char *out, size_t out_size);

/* Stable, human-readable description of a result code (never NULL). */
const char *retropicoes_path_result_str(retropicoes_path_result_t result);

#endif /* RETROPICOES_PATH_MAPPING_H */
