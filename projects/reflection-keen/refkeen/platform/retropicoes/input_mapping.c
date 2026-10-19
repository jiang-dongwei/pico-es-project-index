#include "input_mapping.h"

/* Values come from the port's SDL3 shim header, which is itself checked
 * against a real SDL3 3.4.16 SDL_scancode.h on every run of
 * refkeen/scripts/Test-InputScancodeContract.ps1. Using SDL_SCANCODE_* here
 * rather than bare numbers keeps the two from drifting apart. */
#include <SDL3/SDL_scancode.h>

/* Indexed by compat_key_t. Order follows the enum in compat.h; keep the two in
 * step -- the host test asserts every channel resolves to the value below. */
static const uint16_t g_refkeen_channel_scancodes[COMPAT_KEY_COUNT] = {
    [COMPAT_KEY_LEFT] = SDL_SCANCODE_LEFT,
    [COMPAT_KEY_RIGHT] = SDL_SCANCODE_RIGHT,
    [COMPAT_KEY_UP] = SDL_SCANCODE_UP,
    [COMPAT_KEY_DOWN] = SDL_SCANCODE_DOWN,
    /* "Jumping" in Keen Dreams' own control panel. */
    [COMPAT_KEY_JUMP] = SDL_SCANCODE_LALT,
    /* "Throwing". See the naming-trap note in input_mapping.h. */
    [COMPAT_KEY_ACTION] = SDL_SCANCODE_LCTRL,
    [COMPAT_KEY_START] = SDL_SCANCODE_RETURN,
    [COMPAT_KEY_BACK] = SDL_SCANCODE_ESCAPE,
};

uint16_t retropicoes_refkeen_scancode_for(compat_key_t key)
{
    if ((unsigned)key >= (unsigned)COMPAT_KEY_COUNT)
        return SDL_SCANCODE_UNKNOWN;
    return g_refkeen_channel_scancodes[(unsigned)key];
}

size_t retropicoes_refkeen_scancodes_from_mask(uint32_t compat_mask,
                                               uint16_t *out, size_t capacity)
{
    size_t written = 0u;
    unsigned channel;

    if (out == NULL)
        return 0u;

    for (channel = 0u; channel < (unsigned)COMPAT_KEY_COUNT; ++channel) {
        uint16_t scancode;
        if ((compat_mask & (1u << channel)) == 0u)
            continue;
        scancode = g_refkeen_channel_scancodes[channel];
        if (scancode == SDL_SCANCODE_UNKNOWN)
            continue;
        if (written >= capacity)
            break;
        out[written++] = scancode;
    }
    return written;
}
