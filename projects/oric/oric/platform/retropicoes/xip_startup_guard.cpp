/* SPDX-License-Identifier: GPL-3.0-or-later */
#if defined(ORIC_XIP_GUARD_TEST)
#include "xip_guard_test_shim.h"
#else
#include "hardware/clocks.h"
#include "hardware/structs/qmi.h"
#include "hardware/regs/qmi.h"
#include "hardware/structs/pads_qspi.h"
#include "hardware/sync.h"
#include "pico/error.h"
#include "pico/platform.h"
#endif

namespace Frens {
void configQspiPads();
int enableFlashQuad6BForClock(uint32_t sys_hz);
}
extern "C" int __real_psram_reinitialize(void);

static void __no_inline_not_in_flash_func(drain_startup_rx)(void)
{
    for (unsigned i = 0; i < 16 &&
         !(qmi_hw->direct_csr & QMI_DIRECT_CSR_RXEMPTY_BITS); ++i)
        (void)qmi_hw->direct_rx;
}

/* Before main, or before core1/SD/allocators start. SDK reinitialize() replays
 * boot2 and then stores psram_initialized. Both that store and this caller
 * must execute from SRAM: boot2's temporary M0 mode is not the validated HW16
 * runtime mode. Interrupt exclusion alone cannot protect instruction fetches.
 * The ELF gate also checks that the real SDK function remains in SRAM. */
extern "C" int __no_inline_not_in_flash_func(__wrap_psram_reinitialize)(void)
{
    const uint32_t sys_hz = clock_get_hz(clk_sys); // safe before transition
    const uint32_t irq = save_and_disable_interrupts();
    const uint32_t saved_timing = qmi_hw->m[0].timing;
    const uint32_t saved_rcmd = qmi_hw->m[0].rcmd;
    const uint32_t saved_rfmt = qmi_hw->m[0].rfmt;
    uint32_t saved_pads[6];
    for (unsigned i = 0; i < 6; ++i) saved_pads[i] = pads_qspi_hw->io[i];

    drain_startup_rx();
    int result = __real_psram_reinitialize();
    // Only SRAM/ROM functions and stack data from here until M0 is restored.
    if (result == PICO_OK) {
        drain_startup_rx();
        Frens::configQspiPads();
        const int quad = Frens::enableFlashQuad6BForClock(sys_hz);
        if (quad <= 0) {
            // Restore the working entry configuration, reject initialization;
            // never fake capacity/availability or retry indefinitely.
            for (unsigned i = 0; i < 6; ++i) pads_qspi_hw->io[i] = saved_pads[i];
            qmi_hw->m[0].timing = saved_timing;
            qmi_hw->m[0].rcmd = saved_rcmd;
            qmi_hw->m[0].rfmt = saved_rfmt;
            result = PICO_ERROR_GENERIC;
        }
    }
    __dsb();
    __isb();
    restore_interrupts(irq);
    return result;
}
