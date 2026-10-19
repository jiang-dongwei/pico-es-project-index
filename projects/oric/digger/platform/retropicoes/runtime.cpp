#include <cstdlib>
#include <cstring>
#include <limits>

extern "C" {
#include "compat.h"
#include "ff.h"
}

#include "FrensHelpers.h"
#include "hardware/clocks.h"
#include "hardware/psram.h"
#include "psram_timing.h"
#include "hardware/regs/qmi.h"
#include "hardware/structs/qmi.h"
#include "hardware/vreg.h"
#include "pico/stdlib.h"

#include "runtime.h"
#include "audio_backend.h"
#include "input_backend.h"
#include "video_backend.h"

namespace Frens {
bool initPsram();
void configQspiPads();
int enableFlashQuad6B();
}

namespace {
#ifndef RETROPICOES_BUILD_BANNER
#define RETROPICOES_BUILD_BANNER "DIGGER BUILD: s4.0-candidate"
#endif
#ifndef RETROPICOES_SYSTEM_CLOCK_KHZ
#define RETROPICOES_SYSTEM_CLOCK_KHZ 252000u
#endif
constexpr uint32_t kSystemClockKHz = RETROPICOES_SYSTEM_CLOCK_KHZ;
constexpr size_t kExpectedPsramSize = 8u * 1024u * 1024u;
constexpr size_t kPsramAllocationThreshold = 4096u;

bool g_initialized;
bool g_storage_ready;
bool g_psram_ready;
bool g_video_ready;
bool g_input_ready;
size_t g_psram_size;
bool g_launched_from_bootloader;

uint32_t __no_inline_not_in_flash_func(drain_qmi_rx_fifo)()
{
    uint32_t drained = 0u;

    // pico-bootLoader performs direct QMI transfers before its VTOR jump.
    // The direct RX FIFO is not reset by that jump. psram_reinitialize()
    // expects an empty FIFO, so discard any inherited response bytes before
    // it starts the CS1 quad-enable transaction. Bound the loop defensively;
    // the hardware FIFO is much smaller than this limit.
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_RXEMPTY_BITS) == 0u &&
           drained < 16u) {
        (void)qmi_hw->direct_rx;
        ++drained;
    }
    return drained;
}

bool validate_hw16_psram_timing()
{
    // Frens::initPsram already configures PSRAM from the live system clock.
    // Do not rerun boot2 or overwrite that timing with 252-MHz constants.
    const frens_psram_timing_t expected =
        frens_psram_timing_for_clock(clock_get_hz(clk_sys), XTX_PSRAM_DIVISOR);
    const uint32_t timing = qmi_hw->m[1].timing;
    const uint32_t divisor = (timing & QMI_M1_TIMING_CLKDIV_BITS) >>
                             QMI_M1_TIMING_CLKDIV_LSB;
    const uint32_t rxdelay = (timing & QMI_M1_TIMING_RXDELAY_BITS) >>
                             QMI_M1_TIMING_RXDELAY_LSB;
    const uint32_t min_deselect = (timing & QMI_M1_TIMING_MIN_DESELECT_BITS) >>
                                  QMI_M1_TIMING_MIN_DESELECT_LSB;
    const uint32_t max_select = (timing & QMI_M1_TIMING_MAX_SELECT_BITS) >>
                                QMI_M1_TIMING_MAX_SELECT_LSB;
    const uint32_t cooldown = (timing & QMI_M1_TIMING_COOLDOWN_BITS) >>
                              QMI_M1_TIMING_COOLDOWN_LSB;
    const bool flash_ok =
        ((qmi_hw->m[0].rcmd & QMI_M0_RCMD_PREFIX_BITS) >>
         QMI_M0_RCMD_PREFIX_LSB) == 0x6bu;
    const bool timing_ok = divisor == expected.divisor &&
        rxdelay == expected.rxdelay && min_deselect == expected.min_deselect &&
        max_select == expected.max_select && cooldown == 0u && flash_ok;

    printf("PSRAM HW16: div=%u rxd=%u mindesel=%u maxsel=%u cooldown=%u %s\n",
           (unsigned)divisor, (unsigned)rxdelay, (unsigned)min_deselect,
           (unsigned)max_select, (unsigned)cooldown, timing_ok ? "PASS" : "FAIL");
    printf("S2B FLASH: mode=0x6b %s\n", flash_ok ? "PASS" : "FAIL");
    return timing_ok;
}
}

extern "C" void *frens_f_malloc(size_t size);
extern "C" void frens_f_free(void *ptr);

extern "C" int compat_init(void)
{
    if (g_initialized) {
        return g_storage_ready && g_psram_ready && g_video_ready &&
                       g_input_ready &&
                       g_psram_size == kExpectedPsramSize
                   ? 0
                   : -1;
    }

    Frens::setClocksAndStartStdio(
        kSystemClockKHz,
        kSystemClockKHz >= 378000u ? vreg_voltage::VREG_VOLTAGE_1_40
                                   : vreg_voltage::VREG_VOLTAGE_1_20);
    g_launched_from_bootloader = Frens::isLaunchedFromBootloader();
    const uint32_t qmi_rx_drained = drain_qmi_rx_fifo();
#if RETROPICOES_BOOTLOADER_IMAGE
    printf("%s RetroPicoES v2.1 HW16 Loader image\n",
           RETROPICOES_BUILD_BANNER);
#else
    printf("%s RetroPicoES v2.1 HW16 standalone image\n",
           RETROPICOES_BUILD_BANNER);
#endif
    printf("S2G BOOT: launched_by_loader=%d qmi_rx_drained=%u\n",
           g_launched_from_bootloader ? 1 : 0,
           static_cast<unsigned>(qmi_rx_drained));
    printf("Clock: clk_sys=%u Hz clk_peri=%u Hz\n",
           static_cast<unsigned>(clock_get_hz(clk_sys)),
           static_cast<unsigned>(clock_get_hz(clk_peri)));

    Frens::configQspiPads();
    g_psram_ready = Frens::initPsram();
    if (g_psram_ready) {
        g_psram_ready = validate_hw16_psram_timing();
    }
    g_psram_size = g_psram_ready ? psram_get_size() : 0u;
    printf("PSRAM: ready=%d size=%u bytes expected=%u\n",
           g_psram_ready ? 1 : 0,
           static_cast<unsigned>(g_psram_size),
           static_cast<unsigned>(kExpectedPsramSize));

    if (!g_psram_ready || g_psram_size != kExpectedPsramSize) {
        // Do not let FatFS/shared allocation proceed with unusable PSRAM.
        // Loader builds recover to the picker instead of silently hanging.
        printf("BOOT ERROR: PSRAM unavailable; SD/video/game initialization skipped\n");
        stdio_flush();
        if (g_launched_from_bootloader) {
            sleep_ms(250);
            Frens::rebootToBootloader();
        }
        return -1;
    }

    g_storage_ready = Frens::initSDCard();
    printf("SD: mounted=%d root=/\n", g_storage_ready ? 1 : 0);

    g_video_ready = g_psram_ready && retropicoes_video_init() == 0;
    printf("VIDEO: ready=%d content=320x240 signal=640x480 draw=%s "
           "core1=HSTX\n",
           g_video_ready ? 1 : 0,
           g_video_ready && retropicoes_pointer_in_psram(
                                compat_framebuffer()->pixels)
               ? "PSRAM"
               : "invalid");

    g_input_ready = retropicoes_input_init() == 0;
    printf("INPUT: ready=%d sources=USB1/USB2/NES1/NES2\n",
           g_input_ready ? 1 : 0);

    g_initialized = true;
    return g_storage_ready && g_psram_ready && g_video_ready &&
                   g_input_ready &&
                   g_psram_size == kExpectedPsramSize
               ? 0
               : -1;
}

extern "C" void compat_shutdown(void)
{
    compat_audio_stop();
    if (g_input_ready) {
        retropicoes_input_shutdown();
    }
    if (g_video_ready) {
        retropicoes_video_shutdown();
    }
    if (g_storage_ready) {
        (void)f_unmount("");
    }
    g_storage_ready = false;
    g_video_ready = false;
    g_input_ready = false;
    g_initialized = false;
    g_launched_from_bootloader = false;
}

extern "C" uint32_t compat_ticks_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

extern "C" uint64_t compat_ticks_us(void)
{
    return time_us_64();
}

extern "C" void compat_delay_ms(uint32_t milliseconds)
{
    sleep_ms(milliseconds);
}

extern "C" void *compat_malloc(size_t size)
{
    if (size == 0u) {
        return nullptr;
    }
    if (g_psram_ready && size >= kPsramAllocationThreshold) {
        return frens_f_malloc(size);
    }
    return std::malloc(size);
}

extern "C" void *compat_calloc(size_t count, size_t size)
{
    if (count == 0u || size == 0u) {
        return nullptr;
    }
    if (size > std::numeric_limits<size_t>::max() / count) {
        return nullptr;
    }

    const size_t total = count * size;
    if (g_psram_ready && total >= kPsramAllocationThreshold) {
        void *ptr = frens_f_malloc(total);
        if (ptr != nullptr) {
            std::memset(ptr, 0, total);
        }
        return ptr;
    }
    return std::calloc(count, size);
}

extern "C" void *compat_large_malloc(size_t size)
{
    if (size == 0u) {
        return nullptr;
    }
    if (g_psram_ready) {
        return frens_f_malloc(size);
    }
    return std::malloc(size);
}

extern "C" void *compat_large_calloc(size_t count, size_t size)
{
    if (count == 0u || size == 0u ||
        size > std::numeric_limits<size_t>::max() / count) {
        return nullptr;
    }

    const size_t total = count * size;
    void *ptr = compat_large_malloc(total);
    if (ptr != nullptr) {
        std::memset(ptr, 0, total);
    }
    return ptr;
}

extern "C" void compat_free(void *ptr)
{
    if (ptr == nullptr) {
        return;
    }
    if (g_psram_ready && psram_check_address(ptr)) {
        frens_f_free(ptr);
    } else {
        std::free(ptr);
    }
}

extern "C" int retropicoes_storage_ready(void)
{
    return g_storage_ready ? 1 : 0;
}

extern "C" int retropicoes_psram_ready(void)
{
    return g_psram_ready ? 1 : 0;
}

extern "C" size_t retropicoes_psram_size(void)
{
    return g_psram_size;
}

extern "C" uint32_t retropicoes_system_clock_hz(void)
{
    return clock_get_hz(clk_sys);
}

extern "C" int retropicoes_pointer_in_psram(const void *ptr)
{
    return ptr != nullptr && g_psram_ready &&
                   psram_check_address(const_cast<void *>(ptr))
               ? 1
               : 0;
}

extern "C" int retropicoes_launched_from_bootloader(void)
{
    return g_launched_from_bootloader ? 1 : 0;
}

extern "C" void retropicoes_return_to_bootloader(void)
{
    printf("S2G BOOT: Start hold return requested\n");
    stdio_flush();
    Frens::rebootToBootloader();
}
