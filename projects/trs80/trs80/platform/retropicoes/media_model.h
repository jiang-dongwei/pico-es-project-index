/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TRS80_MEDIA_MODEL_H
#define TRS80_MEDIA_MODEL_H
#include <string.h>
#include "trs80.h"
static inline trs80_model_t trs80_media_model(const char *directory, trs80_model_t fallback)
{
    static const struct { const char *path; trs80_model_t model; } folders[] = {
        {"/roms/TRS80/Model_I", TRS80_MODEL_I},
        {"/roms/TRS80/Model_III", TRS80_MODEL_III},
        {"/roms/TRS80/Model_4", TRS80_MODEL_4}
    };
    for (unsigned i=0; i<sizeof(folders)/sizeof(folders[0]); ++i) {
        size_t n=strlen(folders[i].path);
        if (!strncmp(directory,folders[i].path,n) && (directory[n]=='/' || !directory[n]))
            return folders[i].model;
    }
    return fallback;
}
#endif
