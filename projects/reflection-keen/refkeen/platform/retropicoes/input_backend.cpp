#include <cstdint>
#include <cstdio>

extern "C" {
#include "compat.h"
#include "input_backend.h"
#include "pio_usb.h"
#include "tusb.h"
}

#include "gamepad.h"
#include "nespad.h"

namespace Frens {
void pio_usb_board_init(void);
}

namespace {
bool g_initialized;
bool g_ready;
bool g_menu_held;
bool g_menu_pressed;
uint32_t g_current_mask;
uint32_t g_pressed_mask;
uint32_t g_previous_mask;

constexpr uint32_t key_bit(compat_key_t key)
{
    return 1u << static_cast<unsigned>(key);
}

uint32_t map_usb(uint32_t buttons)
{
    using Button = io::GamePadState::Button;
    uint32_t mapped = 0u;
    if (buttons & Button::LEFT) mapped |= key_bit(COMPAT_KEY_LEFT);
    if (buttons & Button::RIGHT) mapped |= key_bit(COMPAT_KEY_RIGHT);
    if (buttons & Button::UP) mapped |= key_bit(COMPAT_KEY_UP);
    if (buttons & Button::DOWN) mapped |= key_bit(COMPAT_KEY_DOWN);
    if (buttons & (Button::B | Button::Y)) mapped |= key_bit(COMPAT_KEY_JUMP);
    if (buttons & (Button::A | Button::X)) mapped |= key_bit(COMPAT_KEY_ACTION);
    if (buttons & Button::START) mapped |= key_bit(COMPAT_KEY_START);
    if (buttons & Button::SELECT) mapped |= key_bit(COMPAT_KEY_BACK);
    return mapped;
}

uint32_t map_nes(uint16_t buttons, uint8_t pad_type)
{
    enum : uint16_t {
        PAD_B = 1u << 0,
        PAD_A = 1u << 1,
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
    if (buttons & PAD_LEFT) mapped |= key_bit(COMPAT_KEY_LEFT);
    if (buttons & PAD_RIGHT) mapped |= key_bit(COMPAT_KEY_RIGHT);
    if (buttons & PAD_UP) mapped |= key_bit(COMPAT_KEY_UP);
    if (buttons & PAD_DOWN) mapped |= key_bit(COMPAT_KEY_DOWN);
    if (buttons & PAD_SELECT) mapped |= key_bit(COMPAT_KEY_BACK);
    if (buttons & PAD_START) mapped |= key_bit(COMPAT_KEY_START);
    if (pad_type == NESPAD_TYPE_SNES) {
        if (buttons & (PAD_B | PAD_A)) mapped |= key_bit(COMPAT_KEY_JUMP);
        if (buttons & (SNES_A | SNES_X)) mapped |= key_bit(COMPAT_KEY_ACTION);
    } else {
        if (buttons & PAD_A) mapped |= key_bit(COMPAT_KEY_JUMP);
        if (buttons & PAD_B) mapped |= key_bit(COMPAT_KEY_ACTION);
    }
    return mapped;
}
}

extern "C" int retropicoes_input_init(void)
{
    if (g_initialized)
        return g_ready ? 0 : -1;

    const bool nes0 = nespad_begin(0, 252000u, NES_PIN_CLK, NES_PIN_DATA,
                                  NES_PIN_LAT, NES_PIO);
    const bool nes1 = nespad_begin(1, 252000u, NES_PIN_CLK_1, NES_PIN_DATA_1,
                                  NES_PIN_LAT_1, NES_PIO_1);

    Frens::pio_usb_board_init();
    tusb_rhport_init_t host_init = {};
    host_init.role = TUSB_ROLE_HOST;
    host_init.speed = TUSB_SPEED_AUTO;
    const bool usb0 = tusb_init(BOARD_TUH_RHPORT, &host_init);
    const int usb1 = pio_usb_host_add_port(PICO_SECOND_PIO_USB_DP_PIN,
                                           PIO_USB_PINOUT_DPDM);

    g_initialized = true;
    g_ready = nes0 && nes1 && usb0 && usb1 >= 0;
    g_menu_held = false;
    g_menu_pressed = false;
    g_current_mask = g_pressed_mask = g_previous_mask = 0u;

    printf("REFKEEN INPUT: nes=%d/%d usb=%d/%d\n",
           nes0 ? 1 : 0, nes1 ? 1 : 0, usb0 ? 1 : 0, usb1);
    return g_ready ? 0 : -1;
}

extern "C" void retropicoes_input_shutdown(void)
{
    g_initialized = g_ready = false;
    g_menu_held = g_menu_pressed = false;
    g_current_mask = g_pressed_mask = g_previous_mask = 0u;
}

extern "C" int retropicoes_input_ready(void)
{
    return g_ready ? 1 : 0;
}

extern "C" void compat_poll_input(void)
{
    if (!g_initialized) {
        g_current_mask = g_pressed_mask = 0u;
        g_menu_held = g_menu_pressed = false;
        return;
    }

    nespad_read_start();
    tuh_task();
    nespad_read_finish();

    io::GamePadState &usb0 = io::getCurrentGamePadState(0);
    io::GamePadState &usb1 = io::getCurrentGamePadState(1);
    const uint32_t usb_buttons = usb0.buttons | usb1.buttons;
    const bool menu_held = (usb_buttons & io::GamePadState::Button::MENU) != 0u;
    g_menu_pressed = menu_held && !g_menu_held;
    g_menu_held = menu_held;

    g_current_mask = map_usb(usb0.buttons) | map_usb(usb1.buttons) |
        map_nes(nespad_states_ext[0], nespad_padtype[0]) |
        map_nes(nespad_states_ext[1], nespad_padtype[1]);
    g_pressed_mask = g_current_mask & ~g_previous_mask;
    g_previous_mask = g_current_mask;
}

extern "C" int compat_key_down(compat_key_t key)
{
    if (static_cast<unsigned>(key) >= COMPAT_KEY_COUNT)
        return 0;
    return (g_current_mask & key_bit(key)) != 0u;
}

extern "C" int compat_key_pressed(compat_key_t key)
{
    if (static_cast<unsigned>(key) >= COMPAT_KEY_COUNT)
        return 0;
    return (g_pressed_mask & key_bit(key)) != 0u;
}

extern "C" int retropicoes_input_menu_pressed(void)
{
    return g_menu_pressed ? 1 : 0;
}

extern "C" void BEL_ST_FillJoysticksList(void)
{
    /* Physical USB and NES/SNES devices are sampled by the compat backend;
     * upstream's SDL joystick inventory must remain empty. */
}
