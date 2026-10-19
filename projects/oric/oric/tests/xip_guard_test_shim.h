/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstdint>
struct TestWindow { uint32_t timing, rcmd, rfmt; };
struct TestQmi { TestWindow m[2]; uint32_t direct_csr, direct_rx; };
struct TestPads { uint32_t io[6]; };
extern TestQmi *qmi_hw;
extern TestPads *pads_qspi_hw;
constexpr uint32_t QMI_DIRECT_CSR_RXEMPTY_BITS = 1;
constexpr int PICO_OK = 0, PICO_ERROR_GENERIC = -1;
constexpr int clk_sys = 5;
#define __no_inline_not_in_flash_func(name) name
uint32_t clock_get_hz(int);
uint32_t save_and_disable_interrupts();
void restore_interrupts(uint32_t);
inline void __dsb() {}
inline void __isb() {}
