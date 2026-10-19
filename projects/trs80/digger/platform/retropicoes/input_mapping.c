#include "input_mapping.h"

#include "compat.h"

static uint32_t compat_key_bit(compat_key_t key)
{
    return 1u << (unsigned)key;
}

uint32_t retropicoes_map_usb_buttons(uint32_t buttons)
{
    uint32_t mapped = 0u;

    if (buttons & RETROPICOES_USB_LEFT)
        mapped |= compat_key_bit(COMPAT_KEY_LEFT);
    if (buttons & RETROPICOES_USB_RIGHT)
        mapped |= compat_key_bit(COMPAT_KEY_RIGHT);
    if (buttons & RETROPICOES_USB_UP)
        mapped |= compat_key_bit(COMPAT_KEY_UP);
    if (buttons & RETROPICOES_USB_DOWN)
        mapped |= compat_key_bit(COMPAT_KEY_DOWN);
    if (buttons & (RETROPICOES_USB_B | RETROPICOES_USB_Y))
        mapped |= compat_key_bit(COMPAT_KEY_JUMP);
    if (buttons & (RETROPICOES_USB_A | RETROPICOES_USB_X))
        mapped |= compat_key_bit(COMPAT_KEY_ACTION);
    if (buttons & RETROPICOES_USB_START)
        mapped |= compat_key_bit(COMPAT_KEY_START);
    if (buttons & RETROPICOES_USB_SELECT)
        mapped |= compat_key_bit(COMPAT_KEY_BACK);
    return mapped;
}

uint32_t retropicoes_map_nes_buttons(uint16_t buttons, uint8_t pad_type)
{
    enum {
        PAD_BUTTON_0 = 1u << 0,
        PAD_BUTTON_1 = 1u << 1,
        PAD_SELECT = 1u << 2,
        PAD_START = 1u << 3,
        PAD_UP = 1u << 4,
        PAD_DOWN = 1u << 5,
        PAD_LEFT = 1u << 6,
        PAD_RIGHT = 1u << 7,
        SNES_A = 1u << 8,
        SNES_X = 1u << 9
    };
    uint32_t mapped = 0u;

    if (buttons & PAD_LEFT) mapped |= compat_key_bit(COMPAT_KEY_LEFT);
    if (buttons & PAD_RIGHT) mapped |= compat_key_bit(COMPAT_KEY_RIGHT);
    if (buttons & PAD_UP) mapped |= compat_key_bit(COMPAT_KEY_UP);
    if (buttons & PAD_DOWN) mapped |= compat_key_bit(COMPAT_KEY_DOWN);
    if (buttons & PAD_SELECT) mapped |= compat_key_bit(COMPAT_KEY_BACK);
    if (buttons & PAD_START) mapped |= compat_key_bit(COMPAT_KEY_START);

    if (pad_type == RETROPICOES_NESPAD_SNES) {
        if (buttons & (PAD_BUTTON_0 | PAD_BUTTON_1))
            mapped |= compat_key_bit(COMPAT_KEY_JUMP);
        if (buttons & (SNES_A | SNES_X))
            mapped |= compat_key_bit(COMPAT_KEY_ACTION);
    } else {
        /* UNKNOWN is intentionally NES-compatible until SNES-only bits prove
         * the connected controller type. */
        if (buttons & PAD_BUTTON_1)
            mapped |= compat_key_bit(COMPAT_KEY_JUMP);
        if (buttons & PAD_BUTTON_0)
            mapped |= compat_key_bit(COMPAT_KEY_ACTION);
    }
    return mapped;
}

uint32_t retropicoes_input_edge_mask(uint32_t current, uint32_t previous)
{
    return current & ~previous;
}

int retropicoes_loader_return_button_down(uint32_t current)
{
    return (current & compat_key_bit(COMPAT_KEY_START)) != 0u;
}

int retropicoes_loader_return_hold_elapsed(uint32_t now_ms,
                                           uint32_t hold_begin_ms)
{
    /* Unsigned subtraction deliberately handles the 32-bit tick wrap. */
    return (uint32_t)(now_ms - hold_begin_ms) >=
           RETROPICOES_LOADER_RETURN_HOLD_MS;
}
