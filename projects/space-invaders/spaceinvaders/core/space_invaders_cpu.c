/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "space_invaders_cpu.h"

#include <stddef.h>

static uint8_t bridge_read(void *context, uint16_t address)
{
    space_invaders_cpu_t *machine = (space_invaders_cpu_t *)context;
    return machine->bus.read8(machine->bus.context, address);
}

static void bridge_write(void *context, uint16_t address, uint8_t value)
{
    space_invaders_cpu_t *machine = (space_invaders_cpu_t *)context;
    machine->bus.write8(machine->bus.context, address, value);
}

static uint8_t bridge_port_in(void *context, uint8_t port)
{
    space_invaders_cpu_t *machine = (space_invaders_cpu_t *)context;
    return machine->bus.port_in(machine->bus.context, port);
}

static void bridge_port_out(void *context, uint8_t port, uint8_t value)
{
    space_invaders_cpu_t *machine = (space_invaders_cpu_t *)context;
    machine->bus.port_out(machine->bus.context, port, value);
}

static void bind_bus(space_invaders_cpu_t *machine)
{
    machine->cpu.read_byte = bridge_read;
    machine->cpu.write_byte = bridge_write;
    machine->cpu.port_in = bridge_port_in;
    machine->cpu.port_out = bridge_port_out;
    machine->cpu.userdata = machine;
}

bool space_invaders_cpu_init(space_invaders_cpu_t *machine,
                             const arcade_cpu_bus_t *bus)
{
    if (machine == NULL || !arcade_cpu_bus_valid(bus))
        return false;
    machine->bus = *bus;
    i8080_init(&machine->cpu);
    bind_bus(machine);
    return true;
}

void space_invaders_cpu_reset(space_invaders_cpu_t *machine)
{
    if (machine == NULL)
        return;
    i8080_init(&machine->cpu);
    bind_bus(machine);
}

unsigned long space_invaders_cpu_step(space_invaders_cpu_t *machine)
{
    unsigned long before;
    if (machine == NULL)
        return 0ul;
    before = machine->cpu.cyc;
    i8080_step(&machine->cpu);
    return machine->cpu.cyc - before;
}

void space_invaders_cpu_interrupt(space_invaders_cpu_t *machine,
                                  uint8_t rst_opcode)
{
    if (machine != NULL)
        i8080_interrupt(&machine->cpu, rst_opcode);
}
