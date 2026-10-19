/*
 * author : Shuichi TAKANO
 * since  : Sun Jan 02 2022 18:19:28
 */

#include "gamepad.h"

#include <stdio.h>

namespace io
{
    namespace
    {
        GamePadState currentGamePad_[2];
        KeyboardState currentKeyboard_ = {};
        MouseState currentMouse_;
    }

    GamePadState &getCurrentGamePadState(int i)
    {
        return currentGamePad_[i];
    }

    const KeyboardState &getCurrentKeyboardState()
    {
        return currentKeyboard_;
    }

    MouseState &getCurrentMouseState()
    {
        return currentMouse_;
    }

    void
    GamePadState::convertButtonsFromAxis(int axisX, int axisY)
    {
        int x = axis[axisX];
        int y = axis[axisY];

        if (x < 64)
        {
            buttons |= Button::LEFT;
        }
        else if (x > 192)
        {
            buttons |= Button::RIGHT;
        }

        if (y < 64)
        {
            buttons |= Button::UP;
        }
        else if (y > 192)
        {
            buttons |= Button::DOWN;
        }
    }

    void
    GamePadState::convertButtonsFromHat()
    {
        static constexpr int table[] = {
            Button::UP,
            Button::UP | Button::RIGHT,
            Button::RIGHT,
            Button::DOWN | Button::RIGHT,
            Button::DOWN,
            Button::LEFT | Button::DOWN,
            Button::LEFT,
            Button::LEFT | Button::UP,
        };
        auto i = static_cast<int>(hat);
        if (i < 8)
        {
            buttons |= table[i];
        }
    }

    bool
    GamePadState::updateFromHoriPokkenReport(const uint8_t *report,
                                             uint16_t len)
    {
        // Canonical HORIPAD report (no report ID):
        //   [0] Y/B/A/X/L/R/ZL/ZR, [1] -/+/L3/R3/Home/Capture,
        //   [2] hat (0..7, 15 released), [3..6] axes, [7] vendor byte.
        if (!report || len < 8)
        {
            return false;
        }

        const uint8_t face = report[0];
        const uint8_t system = report[1];
        buttons =
            (face & 0x04 ? Button::A : 0) |
            (face & 0x02 ? Button::B : 0) |
            (face & 0x08 ? Button::X : 0) |
            (face & 0x01 ? Button::Y : 0) |
            (face & (0x10 | 0x40) ? Button::L : 0) |
            (face & (0x20 | 0x80) ? Button::R : 0) |
            (system & 0x01 ? Button::SELECT : 0) |
            (system & 0x02 ? Button::START : 0);

        axis[0] = report[3];
        axis[1] = report[4];
        axis[2] = report[5];
        const uint8_t hatValue = report[2] & 0x0f;
        hat = hatValue < 8 ? static_cast<Hat>(hatValue) : Hat::RELEASED;
        convertButtonsFromHat();
        flagConnected(true);
        return true;
    }
}
