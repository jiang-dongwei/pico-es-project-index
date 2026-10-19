#ifndef SDLPOP_RETROPICOES_INPUT_MAPPING_H
#define SDLPOP_RETROPICOES_INPUT_MAPPING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mirrors pico_shared/io::GamePadState::Button without exposing C tests to C++. */
enum {
    RETROPICOES_USB_A = 1u << 0,
    RETROPICOES_USB_B = 1u << 1,
    RETROPICOES_USB_X = 1u << 2,
    RETROPICOES_USB_Y = 1u << 3,
    RETROPICOES_USB_SELECT = 1u << 6,
    RETROPICOES_USB_START = 1u << 7,
    RETROPICOES_USB_MENU = 1u << 10,
    RETROPICOES_USB_LEFT = 1u << 31,
    RETROPICOES_USB_RIGHT = 1u << 30,
    RETROPICOES_USB_UP = 1u << 29,
    RETROPICOES_USB_DOWN = 1u << 28
};

enum {
    RETROPICOES_NESPAD_UNKNOWN = 0,
    RETROPICOES_NESPAD_NES = 1,
    RETROPICOES_NESPAD_SNES = 2
};

#ifndef RETROPICOES_LOADER_RETURN_HOLD_MS
#define RETROPICOES_LOADER_RETURN_HOLD_MS 2000u
#endif

/* Legacy cores may keep the direct Start-hold escape.  Unified-menu cores
 * disable it and route Return Loader through the confirmed menu action. */
#ifndef RETROPICOES_ENABLE_DIRECT_LOADER_RETURN
#define RETROPICOES_ENABLE_DIRECT_LOADER_RETURN 1
#endif

uint32_t retropicoes_map_usb_buttons(uint32_t buttons);
uint32_t retropicoes_map_nes_buttons(uint16_t buttons, uint8_t pad_type);
uint32_t retropicoes_input_edge_mask(uint32_t current, uint32_t previous);
int retropicoes_loader_return_button_down(uint32_t current);
int retropicoes_loader_return_hold_elapsed(uint32_t now_ms,
                                           uint32_t hold_begin_ms);

#ifdef __cplusplus
}
#endif

#endif
