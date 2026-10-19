/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_SPACE_INVADERS_CPU_H
#define RETROPICOES_SPACE_INVADERS_CPU_H

#include <stdbool.h>
#include <stdint.h>

#include "arcade_machine.h"
#include "i8080.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i8080 cpu;
    arcade_cpu_bus_t bus;
} space_invaders_cpu_t;

bool space_invaders_cpu_init(space_invaders_cpu_t *machine,
                             const arcade_cpu_bus_t *bus);
void space_invaders_cpu_reset(space_invaders_cpu_t *machine);
unsigned long space_invaders_cpu_step(space_invaders_cpu_t *machine);
void space_invaders_cpu_interrupt(space_invaders_cpu_t *machine,
                                  uint8_t rst_opcode);

#ifdef __cplusplus
}
#endif
#endif
