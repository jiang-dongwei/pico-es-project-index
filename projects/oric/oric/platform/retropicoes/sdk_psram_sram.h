/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Forced include only for this target's SDK psram.c compilation. Keep the
 * post-flash_start_xip flag store and return off XIP until our wrapper restores
 * the HW16 flash configuration. No edits to the pinned SDK are necessary. */
#include "hardware/psram.h"
int psram_reinitialize(void)
    __attribute__((section(".time_critical.psram_reinitialize"), noinline));
