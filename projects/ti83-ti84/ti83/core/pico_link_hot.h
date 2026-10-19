/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stddef.h>
#include <stdio.h>
#include "tilem.h"
#define LINK_HOT __attribute__((section(".time_critical.ti83_link")))
static void dbus_update(TilemCalc *) LINK_HOT;
void tilem_linkport_set_lines(TilemCalc *,byte) LINK_HOT;
byte tilem_linkport_get_lines(TilemCalc *) LINK_HOT;
byte tilem_linkport_read_byte(TilemCalc *) LINK_HOT;
void tilem_linkport_write_byte(TilemCalc *,byte) LINK_HOT;
