/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "path_adapter.h"
#include <ctype.h>
#include <string.h>
int retropicoes_studio2_rom_profile(const char *name, int *profile) {
    const char *e;
    char ext[5] = {0};
    unsigned i;
    if (!name)
        return 0;
    e = strrchr(name, '.');
    if (!e || strlen(e) > 4)
        return 0;
    for (i = 0; e[i]; i++)
        ext[i] = (char)tolower((unsigned char)e[i]);
    if (strcmp(ext, ".st2") && strcmp(ext, ".bin") && strcmp(ext, ".rom") && strcmp(ext, ".s2d"))
        return 0;
    if (profile)
        *profile = 0;
    return 1;
}
