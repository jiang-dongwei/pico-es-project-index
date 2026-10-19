/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "arcade_machine.h"
#include "space_invaders_cpu.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t memory[65536];
    uint8_t input_value;
    uint8_t input_port;
    uint8_t output_value;
    uint8_t output_port;
    unsigned writes;
} test_bus_t;

static unsigned failures;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

static uint8_t read8(void *context, uint16_t address)
{
    return ((test_bus_t *)context)->memory[address];
}

static void write8(void *context, uint16_t address, uint8_t value)
{
    test_bus_t *bus = (test_bus_t *)context;
    bus->memory[address] = value;
    ++bus->writes;
}

static uint8_t port_in(void *context, uint8_t port)
{
    test_bus_t *bus = (test_bus_t *)context;
    bus->input_port = port;
    return bus->input_value;
}

static void port_out(void *context, uint8_t port, uint8_t value)
{
    test_bus_t *bus = (test_bus_t *)context;
    bus->output_port = port;
    bus->output_value = value;
}

static arcade_cpu_bus_t make_bus(test_bus_t *storage)
{
    arcade_cpu_bus_t bus = {storage, read8, write8, port_in, port_out};
    return bus;
}

static void test_cpu_arithmetic_io_and_cycles(void)
{
    static const uint8_t program[] = {
        0x3e, 0x09,       /* MVI A,09 */
        0xc6, 0x09,       /* ADI 09 */
        0x27,             /* DAA -> packed BCD 18 */
        0xd3, 0x03,       /* OUT 03 */
        0xdb, 0x01,       /* IN 01 */
        0x76              /* HLT */
    };
    test_bus_t storage;
    arcade_cpu_bus_t bus;
    space_invaders_cpu_t machine;
    unsigned long cycles = 0ul;

    memset(&storage, 0, sizeof(storage));
    memcpy(storage.memory, program, sizeof(program));
    storage.input_value = 0xa5u;
    bus = make_bus(&storage);
    CHECK(space_invaders_cpu_init(&machine, &bus));
    for (unsigned i = 0; i < 6u; ++i)
        cycles += space_invaders_cpu_step(&machine);
    CHECK(cycles == 45ul);
    CHECK(storage.output_port == 3u && storage.output_value == 0x18u);
    CHECK(storage.input_port == 1u && machine.cpu.a == 0xa5u);
    CHECK(machine.cpu.halted);
    CHECK(space_invaders_cpu_step(&machine) == 0ul);
}

static void test_memory_contract_and_reset(void)
{
    static const uint8_t program[] = {
        0x21, 0x00, 0x20, /* LXI H,2000 */
        0x36, 0x5a,       /* MVI M,5A */
        0x7e              /* MOV A,M */
    };
    test_bus_t storage;
    arcade_cpu_bus_t bus;
    space_invaders_cpu_t machine;

    memset(&storage, 0, sizeof(storage));
    memcpy(storage.memory, program, sizeof(program));
    bus = make_bus(&storage);
    CHECK(space_invaders_cpu_init(&machine, &bus));
    CHECK(space_invaders_cpu_step(&machine) == 10ul);
    CHECK(space_invaders_cpu_step(&machine) == 10ul);
    CHECK(space_invaders_cpu_step(&machine) == 7ul);
    CHECK(storage.memory[0x2000] == 0x5au && storage.writes == 1u);
    CHECK(machine.cpu.a == 0x5au);
    space_invaders_cpu_reset(&machine);
    CHECK(machine.cpu.pc == 0u && machine.cpu.cyc == 0ul);
    CHECK(machine.cpu.read_byte != NULL && machine.cpu.userdata == &machine);
}

static void test_arithmetic_flags(void)
{
    static const uint8_t program[] = {
        0x3e, 0xff, /* MVI A,FF */
        0xc6, 0x01, /* ADI 01 */
        0x3c,       /* INR A: carry must be preserved */
        0x3d        /* DCR A: carry must be preserved */
    };
    test_bus_t storage;
    arcade_cpu_bus_t bus;
    space_invaders_cpu_t machine;

    memset(&storage, 0, sizeof(storage));
    memcpy(storage.memory, program, sizeof(program));
    bus = make_bus(&storage);
    CHECK(space_invaders_cpu_init(&machine, &bus));
    CHECK(space_invaders_cpu_step(&machine) == 7ul);
    CHECK(space_invaders_cpu_step(&machine) == 7ul);
    CHECK(machine.cpu.a == 0u && machine.cpu.zf && machine.cpu.pf &&
          machine.cpu.cf && machine.cpu.hf && !machine.cpu.sf);
    CHECK(space_invaders_cpu_step(&machine) == 5ul);
    CHECK(machine.cpu.a == 1u && !machine.cpu.zf && !machine.cpu.pf &&
          machine.cpu.cf);
    CHECK(space_invaders_cpu_step(&machine) == 5ul);
    CHECK(machine.cpu.a == 0u && machine.cpu.zf && machine.cpu.pf &&
          machine.cpu.cf);
}

static void test_ei_delay_halt_and_interrupt(void)
{
    static const uint8_t program[] = {0xfb, 0x76}; /* EI; HLT */
    test_bus_t storage;
    arcade_cpu_bus_t bus;
    space_invaders_cpu_t machine;

    memset(&storage, 0, sizeof(storage));
    memcpy(storage.memory, program, sizeof(program));
    bus = make_bus(&storage);
    CHECK(space_invaders_cpu_init(&machine, &bus));
    CHECK(space_invaders_cpu_step(&machine) == 4ul);
    space_invaders_cpu_interrupt(&machine, 0xcfu); /* RST 1 */
    CHECK(space_invaders_cpu_step(&machine) == 7ul);
    CHECK(machine.cpu.halted && machine.cpu.pc == 2u);
    CHECK(space_invaders_cpu_step(&machine) == 11ul);
    CHECK(!machine.cpu.halted && machine.cpu.pc == 0x0008u);
    CHECK(machine.cpu.sp == 0xfffeu);
    CHECK(storage.memory[0xfffe] == 0x02u && storage.memory[0xffff] == 0x00u);
}

static void test_arcade_scheduler(void)
{
    arcade_cycle_scheduler_t scheduler;
    uint64_t total = 0u;

    CHECK(!arcade_cycle_scheduler_init(NULL, 1996800u, 120u));
    CHECK(!arcade_cycle_scheduler_init(&scheduler, 0u, 120u));
    CHECK(arcade_cycle_scheduler_init(&scheduler, 1996800u, 120u));
    for (unsigned tick = 0; tick < 120u; ++tick) {
        uint32_t slice = arcade_cycle_scheduler_next(&scheduler);
        CHECK(slice == 16640u);
        total += slice;
    }
    CHECK(total == 1996800u);
    CHECK(scheduler.scheduled_cycles == total && scheduler.tick_index == 120u);

    CHECK(arcade_cycle_scheduler_init(&scheduler, 5u, 2u));
    CHECK(arcade_cycle_scheduler_next(&scheduler) == 2u);
    CHECK(arcade_cycle_scheduler_next(&scheduler) == 3u);
}

int main(void)
{
    test_bus_t storage;
    arcade_cpu_bus_t bus;

    memset(&storage, 0, sizeof(storage));
    bus = make_bus(&storage);
    CHECK(arcade_cpu_bus_valid(&bus));
    bus.port_out = NULL;
    CHECK(!arcade_cpu_bus_valid(&bus));
    CHECK(!space_invaders_cpu_init(NULL, &bus));

    test_cpu_arithmetic_io_and_cycles();
    test_memory_contract_and_reset();
    test_arithmetic_flags();
    test_ei_delay_halt_and_interrupt();
    test_arcade_scheduler();
    if (failures != 0u) {
        fprintf(stderr, "%u Space Invaders CPU checks failed\n", failures);
        return 1;
    }
    puts("Space Invaders CPU and arcade-template checks passed");
    return 0;
}
