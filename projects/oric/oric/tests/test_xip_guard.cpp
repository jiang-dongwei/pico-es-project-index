/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "xip_guard_test_shim.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
static TestQmi qmi;
static TestPads pads;
TestQmi *qmi_hw = &qmi;
TestPads *pads_qspi_hw = &pads;
static uint32_t irq, initial_irq;
static int real_result, quad_result, sequence;
extern "C" int __wrap_psram_reinitialize();
uint32_t clock_get_hz(int clock) {
    CHECK(clock == clk_sys && irq == initial_irq && sequence++ == 0);
    return 378000000;
}
uint32_t save_and_disable_interrupts() { uint32_t prev = irq; irq = 1; return prev; }
void restore_interrupts(uint32_t value) { irq = value; }
extern "C" int __real_psram_reinitialize() {
    CHECK(irq == 1 && sequence++ == 1);
    if (real_result == 0) { qmi.m[0] = {6, 0xeb, 2}; pads.io[0] = 99; }
    return real_result;
}
namespace Frens {
void configQspiPads() { CHECK(irq == 1 && sequence++ == 2); pads.io[0] = 88; }
int enableFlashQuad6BForClock(uint32_t hz) {
    CHECK(irq == 1 && hz == 378000000 && sequence++ == 3);
    qmi.m[0] = {9, 0x6b, 3};
    return quad_result;
}
}
int main() {
    for (initial_irq = 0; initial_irq <= 1; ++initial_irq) {
        for (int outcome = 0; outcome < 3; ++outcome) {
            irq = initial_irq; sequence = 0;
            real_result = outcome == 2 ? -14 : 0;
            quad_result = outcome == 1 ? 0 : 1;
            qmi = {{{7, 0x6b, 1}, {4, 5, 6}}, 1, 0};
            for (unsigned i = 0; i < 6; ++i) pads.io[i] = i + 10;
            int result = __wrap_psram_reinitialize();
            CHECK(irq == initial_irq);
            if (outcome == 0) {
                CHECK(result == 0 && sequence == 4);
                CHECK(qmi.m[0].timing == 9 && qmi.m[0].rcmd == 0x6b && pads.io[0] == 88);
            } else {
                CHECK(result == (outcome == 1 ? -1 : -14));
                CHECK(sequence == (outcome == 1 ? 4 : 2));
                CHECK(qmi.m[0].timing == 7 && qmi.m[0].rcmd == 0x6b && qmi.m[0].rfmt == 1);
                for (unsigned i = 0; i < 6; ++i) CHECK(pads.io[i] == i + 10);
            }
            CHECK(qmi.m[1].timing == 4 && qmi.m[1].rcmd == 5 && qmi.m[1].rfmt == 6);
        }
    }
    std::puts("XIP guard order, success/failure rollback, nested IRQ preservation: 6/6 PASS");
}
