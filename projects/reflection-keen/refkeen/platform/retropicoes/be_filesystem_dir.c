/* ReflectionHLE filesystem directory seam for RetroPicoES.
 *
 * Upstream deliberately keeps directory enumeration behind four BEL_Cross_*
 * functions. The Pico port implements that seam with compat v1, whose target
 * backend is FatFS. No POSIX/newlib directory API is used here. */

#include "compat.h"
#include "be_filesystem_dir_backend.h"

#include "backend/filesystem/be_filesystem_tchar.h"

/* Deliberately repeat the opaque typedef instead of including
 * be_filesystem_dir.h. That upstream header defines BEL_Cross_IsDir inline but
 * has no Pico arm, producing a missing-return function even in a TU that never
 * calls it. The ABI is simply void * in the upstream declaration. */
typedef void *BE_DIR_T;

BE_DIR_T BEL_Cross_OpenDir(const TCHAR *dirname)
{
    if (dirname == NULL || dirname[0] == '\0')
        return NULL;
    return (BE_DIR_T)compat_dir_open(dirname);
}

TCHAR *BEL_Cross_ReadDir(BE_DIR_T dir)
{
    /* compat_dir_read returns storage owned by the directory object. That is
     * the same lifetime BE_DIR_T promises. The FatFS backend stores the name
     * in a mutable buffer because upstream may temporarily edit it. */
    return (TCHAR *)compat_dir_read((compat_dir_t *)dir);
}

void BEL_Cross_RewindDir(BE_DIR_T dir)
{
    if (dir != NULL)
        retropicoes_compat_dir_rewind((compat_dir_t *)dir);
}

void BEL_Cross_CloseDir(BE_DIR_T dir)
{
    compat_dir_close((compat_dir_t *)dir);
}
