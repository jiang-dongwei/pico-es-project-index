/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stddef.h>
#include <stdio.h>
#include "tilem.h"
byte x4_z80_in(TilemCalc *,dword) __attribute__((section(".time_critical.ti83_io")));
void x4_z80_out(TilemCalc *,dword,byte) __attribute__((section(".time_critical.ti83_io")));
byte tilem_keypad_read_keys(TilemCalc *) __attribute__((section(".time_critical.ti83_keys")));
void tilem_keypad_set_group(TilemCalc *,byte) __attribute__((section(".time_critical.ti83_keys")));
