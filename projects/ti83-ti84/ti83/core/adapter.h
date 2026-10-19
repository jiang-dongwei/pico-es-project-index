/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TI83_ADAPTER_H
#define TI83_ADAPTER_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
#define restrict __restrict
#endif
#include "tilem.h"
#ifdef __cplusplus
#undef restrict
#endif
#include "scancodes.h"
#ifdef __cplusplus
extern "C" {
#endif
int ti83_extension(const char *name);
char ti83_model(const char *name, size_t size);
TilemCalc *ti83_create(char model);
void ti83_step(TilemCalc *calc, unsigned microseconds);
void ti83_release_keys(TilemCalc *calc);
int ti83_blit(TilemCalc *calc, uint16_t *pixels, unsigned stride,
              unsigned width, unsigned height, int keyboard, int scanlines);
extern const unsigned ti83_keycodes[49];
extern const char *const ti83_keylabels[49];
typedef struct { unsigned previous, cursor; uint8_t keyboard, second, alpha, game; uint8_t down[57]; } ti83_input_t;
void ti83_input_reset(ti83_input_t *input, int keyboard);
void ti83_input_update(TilemCalc *calc, ti83_input_t *input, unsigned buttons);
#ifdef __cplusplus
}
#endif
#endif
