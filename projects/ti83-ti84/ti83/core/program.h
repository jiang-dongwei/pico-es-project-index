/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TI83_PROGRAM_H
#define TI83_PROGRAM_H
#include "adapter.h"
typedef struct {
    char name[9];
    uint8_t type, version, assembly;
    const uint8_t *data;
    uint16_t size;
} ti83_program_t;
typedef int (*ti83_progress_t)(void *context);
typedef struct { unsigned stage,completed,total; } ti83_load_progress_t;
void ti83_program_progress_reset(void);
ti83_load_progress_t ti83_program_progress(void);
int ti83_program_extension(const char *path);
int ti83_program_parse(const uint8_t *bytes, size_t size, ti83_program_t *program,
                       char *error, size_t cap);
void ti83_power_on(TilemCalc *calc);
int ti83_power_on_progress(TilemCalc *calc, ti83_progress_t progress, void *context);
int ti83_program_launch(TilemCalc *calc, const ti83_program_t *program,
                        ti83_progress_t progress, void *context,
                        char *error, size_t cap);
#endif
