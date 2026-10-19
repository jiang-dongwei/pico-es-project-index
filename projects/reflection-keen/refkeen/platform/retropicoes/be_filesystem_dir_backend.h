#ifndef REFKEEN_RETROPICOES_FILESYSTEM_DIR_BACKEND_H
#define REFKEEN_RETROPICOES_FILESYSTEM_DIR_BACKEND_H

#include "compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* compat v1 exposes open/read/close. ReflectionHLE additionally requires
 * rewind, so the FatFS backend supplies this narrow extension. */
void retropicoes_compat_dir_rewind(compat_dir_t *directory);

#ifdef __cplusplus
}
#endif

#endif
