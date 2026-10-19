#include <cstdio>
#include <cstring>

extern "C" {
#include "compat.h"
#include "input_backend.h"
#include "input_mapping.h"
#include "runtime.h"
#include "pio_usb.h"
#include "tusb.h"
}

#include "gamepad.h"
#include "nespad.h"

namespace Frens {
void pio_usb_board_init(void);
}

#if !defined(CFG_TUH_RPI_PIO_USB) || !CFG_TUH_RPI_PIO_USB
#error "RetroPicoES S2-D requires the PIO-USB TinyUSB host"
#endif

#if PICO_DEFAULT_PIO_USB_DP_PIN != 28 || PICO_SECOND_PIO_USB_DP_PIN != 26
#error "Unexpected RetroPicoES v2.1 PIO-USB pin map"
#endif

#if NES_PIN_CLK != 6 || NES_PIN_LAT != 7 || NES_PIN_DATA != 8 || \
    NES_PIN_CLK_1 != 9 || NES_PIN_LAT_1 != 10 || NES_PIN_DATA_1 != 11
#error "Unexpected RetroPicoES v2.1 NES controller pin map"
#endif

static_assert(RETROPICOES_USB_A == io::GamePadState::Button::A);
static_assert(RETROPICOES_USB_B == io::GamePadState::Button::B);
static_assert(RETROPICOES_USB_X == io::GamePadState::Button::X);
static_assert(RETROPICOES_USB_Y == io::GamePadState::Button::Y);
static_assert(RETROPICOES_USB_SELECT == io::GamePadState::Button::SELECT);
static_assert(RETROPICOES_USB_START == io::GamePadState::Button::START);
static_assert(RETROPICOES_USB_LEFT ==
              static_cast<uint32_t>(io::GamePadState::Button::LEFT));
static_assert(RETROPICOES_USB_RIGHT ==
              static_cast<uint32_t>(io::GamePadState::Button::RIGHT));
static_assert(RETROPICOES_USB_UP ==
              static_cast<uint32_t>(io::GamePadState::Button::UP));
static_assert(RETROPICOES_USB_DOWN ==
              static_cast<uint32_t>(io::GamePadState::Button::DOWN));
static_assert(static_cast<int>(RETROPICOES_NESPAD_UNKNOWN) ==
              static_cast<int>(NESPAD_TYPE_UNKNOWN));
static_assert(static_cast<int>(RETROPICOES_NESPAD_NES) ==
              static_cast<int>(NESPAD_TYPE_NES));
static_assert(static_cast<int>(RETROPICOES_NESPAD_SNES) ==
              static_cast<int>(NESPAD_TYPE_SNES));

namespace {
bool g_initialized;
bool g_ready;
uint32_t g_previous_mask;
bool g_loader_return_armed;
bool g_loader_start_hold_active;
uint32_t g_loader_start_hold_begin_ms;
retropicoes_input_stats_t g_stats;

uint32_t usb_source(unsigned index)
{
    io::GamePadState &pad = io::getCurrentGamePadState((int)index);
    if (!pad.isConnected()) return 0u;
    g_stats.usb_connected_mask |= (uint8_t)(1u << index);
    return retropicoes_map_usb_buttons(pad.buttons);
}

}

/* Ports that own persistent state may override this hook.  It deliberately
   lives below compat v1 so the frozen public API stays unchanged. */
extern "C" __attribute__((weak)) void retropicoes_before_loader_return(void)
{
}

extern "C" int retropicoes_input_init(void)
{
    if (g_initialized) return g_ready ? 0 : -1;

    std::memset(&g_stats, 0, sizeof(g_stats));
    const bool nes0 = nespad_begin(0, 252000u, NES_PIN_CLK, NES_PIN_DATA,
                                  NES_PIN_LAT, NES_PIO);
    const bool nes1 = nespad_begin(1, 252000u, NES_PIN_CLK_1, NES_PIN_DATA_1,
                                  NES_PIN_LAT_1, NES_PIO_1);
    g_stats.nes_initialized_mask =
        (uint8_t)((nes0 ? 1u : 0u) | (nes1 ? 2u : 0u));

    Frens::pio_usb_board_init();
    tusb_rhport_init_t host_init = {};
    host_init.role = TUSB_ROLE_HOST;
    host_init.speed = TUSB_SPEED_AUTO;
    g_stats.usb_host_initialized =
        tusb_init(BOARD_TUH_RHPORT, &host_init) ? 1 : 0;
    g_stats.usb_second_port_result =
        pio_usb_host_add_port(PICO_SECOND_PIO_USB_DP_PIN,
                              PIO_USB_PINOUT_DPDM);

    g_initialized = true;
    g_loader_return_armed = false;
    g_loader_start_hold_active = false;
    g_loader_start_hold_begin_ms = 0u;
    g_ready = nes0 && nes1 && g_stats.usb_host_initialized &&
              g_stats.usb_second_port_result >= 0;
    printf("INPUT: NES1=%d pins=%d/%d/%d NES2=%d pins=%d/%d/%d PIO=1\n",
           nes0 ? 1 : 0, NES_PIN_CLK, NES_PIN_LAT, NES_PIN_DATA,
           nes1 ? 1 : 0, NES_PIN_CLK_1, NES_PIN_LAT_1, NES_PIN_DATA_1);
    printf("INPUT: PIO-USB host=%d PIO=%d port1_dp=%d port2_dp=%d add_port=%d\n",
           g_stats.usb_host_initialized, PIO_USB_USE_PIO,
           PICO_DEFAULT_PIO_USB_DP_PIN, PICO_SECOND_PIO_USB_DP_PIN,
           g_stats.usb_second_port_result);
    return g_ready ? 0 : -1;
}

extern "C" void retropicoes_input_shutdown(void)
{
    g_initialized = false;
    g_ready = false;
    g_previous_mask = 0u;
    g_loader_return_armed = false;
    g_loader_start_hold_active = false;
    g_loader_start_hold_begin_ms = 0u;
    std::memset(&g_stats, 0, sizeof(g_stats));
}

extern "C" int retropicoes_input_ready(void)
{
    return g_ready ? 1 : 0;
}

extern "C" void compat_poll_input(void)
{
    if (!g_initialized) {
        g_stats.current_mask = 0u;
        g_stats.pressed_mask = 0u;
        return;
    }

    nespad_read_start();
    tuh_task();
    nespad_read_finish();

    g_stats.usb_connected_mask = 0u;
    g_stats.source_masks[0] = usb_source(0u);
    g_stats.source_masks[1] = usb_source(1u);
    g_stats.source_masks[2] = retropicoes_map_nes_buttons(
        nespad_states_ext[0], nespad_padtype[0]);
    g_stats.source_masks[3] = retropicoes_map_nes_buttons(
        nespad_states_ext[1], nespad_padtype[1]);
    g_stats.nes_pad_type[0] = nespad_padtype[0];
    g_stats.nes_pad_type[1] = nespad_padtype[1];

    g_stats.current_mask = g_stats.source_masks[0] |
                           g_stats.source_masks[1] |
                           g_stats.source_masks[2] |
                           g_stats.source_masks[3];

    const bool loader_start_down =
        retropicoes_loader_return_button_down(g_stats.current_mask) != 0;
    if (!loader_start_down) {
        // Ignore a Start press inherited from the Loader launch. Returning is
        // armed only after Digger has observed Start released at least once.
        g_loader_return_armed = true;
        g_loader_start_hold_active = false;
    } else if (g_loader_return_armed &&
               retropicoes_launched_from_bootloader()) {
        const uint32_t now_ms = compat_ticks_ms();
        if (!g_loader_start_hold_active) {
            g_loader_start_hold_active = true;
            g_loader_start_hold_begin_ms = now_ms;
        } else if (retropicoes_loader_return_hold_elapsed(
                       now_ms, g_loader_start_hold_begin_ms)) {
            g_loader_return_armed = false;
            g_loader_start_hold_active = false;
            retropicoes_before_loader_return();
            retropicoes_return_to_bootloader();
        }
    } else {
        g_loader_start_hold_active = false;
    }

    g_stats.pressed_mask =
        retropicoes_input_edge_mask(g_stats.current_mask, g_previous_mask);
    g_previous_mask = g_stats.current_mask;
    ++g_stats.polls;
}

extern "C" int compat_key_down(compat_key_t key)
{
    if ((unsigned)key >= COMPAT_KEY_COUNT) return 0;
    return (g_stats.current_mask & (1u << (unsigned)key)) != 0u;
}

extern "C" int compat_key_pressed(compat_key_t key)
{
    if ((unsigned)key >= COMPAT_KEY_COUNT) return 0;
    return (g_stats.pressed_mask & (1u << (unsigned)key)) != 0u;
}

extern "C" void retropicoes_input_get_stats(retropicoes_input_stats_t *stats)
{
    if (stats != nullptr) *stats = g_stats;
}
