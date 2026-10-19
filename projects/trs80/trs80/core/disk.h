/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TRS80_DISK_H
#define TRS80_DISK_H
#include "trs80.h"
void trs80_disk_reset(void);
void trs80_disk_select(uint8_t value, int model, uint64_t now, uint32_t hz);
uint8_t trs80_disk_read(unsigned reg, uint64_t now, uint32_t hz);
void trs80_disk_write(unsigned reg, uint8_t value, uint64_t now, uint32_t hz);
int trs80_disk_irq(uint64_t now);
uint64_t trs80_disk_wait(uint64_t now);
#endif
