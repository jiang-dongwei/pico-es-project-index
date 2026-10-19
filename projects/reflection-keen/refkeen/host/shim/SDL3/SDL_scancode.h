/* SDL3/SDL_scancode.h -- the Pico port's stand-in for the real scancode enum.
 *
 * WHY THIS FILE EXISTS
 * --------------------
 * The engine never sees an SDL scancode; it consumes a DOS scancode. But the
 * *intermediate* encoding on the way there is SDL's, at two independent places:
 *
 *   1. Live events. src/backend/events/be_events_sdl.c reads
 *      event.key.scancode, indexes sdlKeyMappings[scancode]
 *      (src/backend/events/be_events_sdl_keymap.h) and hands the resulting
 *      emulatedDOSKeyEvent to BEL_ST_HandleEmuKeyboardEvent(). The port
 *      replaces that TU with its own event source, so the port must produce
 *      the same encoding for the same physical key -- otherwise it would have
 *      to maintain a second, diverging copy of the mapping table.
 *
 *   2. Config persistence. For the 2015 release, id_us.c stores the ten
 *      keyboard bindings as int16 SDL scancodes and converts them with
 *      RefKeen_TranslateScancodeFromSDL2ToDOS() /
 *      ...FromDOSToSDL2() (src/kdreams/key_scancode_converters.c, zlib). A
 *      plain DOS-scancode port could not read or write such a config file.
 *
 * So "the port speaks SDL scancodes" is a contract, not a convenience.
 *
 * VALUE PROVENANCE -- MEASURED, NOT TRANSCRIBED
 * ---------------------------------------------
 * Every value below was read out of a real SDL3 header, not from memory:
 *
 *   SDL3-devel-3.4.16-mingw, x86_64-w64-mingw32/include/SDL3/SDL_scancode.h
 *
 * and refkeen/scripts/Test-InputScancodeContract.ps1 re-derives them from that
 * header on every run, so a transcription slip cannot survive. The same script
 * also checks the values against the SDL 2.0.2 enum that upstream embeds in
 * key_scancode_converters.c: the two agree on every scancode Keen Dreams can
 * reach, which is what makes reuse of the upstream table safe.
 *
 * RELATION TO THE REAL KEYMAP TABLE
 * ---------------------------------
 * be_events_sdl_keymap.h indexes its 283-entry table *by numeric SDL scancode*
 * -- verified, not assumed: the ordinal of each annotated entry equals the
 * scancode named in its comment for all 104 annotated entries (measured
 * against real 3.4.16). That is why the constants here have to be exact, and
 * why SDL_SCANCODE_COUNT must be the real 512 rather than a smaller bound: the
 * table is declared with that length.
 *
 * SCOPE: the keys Keen Dreams can actually be bound to, not all 512. Anything
 * missing fails loudly at compile time, which is the desired failure mode.
 */

#ifndef REFKEEN_SDL3_SHIM_SDL_SCANCODE_H
#define REFKEEN_SDL3_SHIM_SDL_SCANCODE_H

typedef enum SDL_Scancode
{
    SDL_SCANCODE_UNKNOWN = 0,

    /* Letters. Values are USB usage page 0x07 ids, so they are contiguous
     * from 'a' and deliberately not A=1. */
    SDL_SCANCODE_A = 4,
    SDL_SCANCODE_B = 5,
    SDL_SCANCODE_C = 6,
    SDL_SCANCODE_D = 7,
    SDL_SCANCODE_E = 8,
    SDL_SCANCODE_F = 9,
    SDL_SCANCODE_G = 10,
    SDL_SCANCODE_H = 11,
    SDL_SCANCODE_I = 12,
    SDL_SCANCODE_J = 13,
    SDL_SCANCODE_K = 14,
    SDL_SCANCODE_L = 15,
    SDL_SCANCODE_M = 16,
    SDL_SCANCODE_N = 17,
    SDL_SCANCODE_O = 18,
    SDL_SCANCODE_P = 19,
    SDL_SCANCODE_Q = 20,
    SDL_SCANCODE_R = 21,
    SDL_SCANCODE_S = 22,
    SDL_SCANCODE_T = 23,
    SDL_SCANCODE_U = 24,
    SDL_SCANCODE_V = 25,
    SDL_SCANCODE_W = 26,
    SDL_SCANCODE_X = 27,
    SDL_SCANCODE_Y = 28,
    SDL_SCANCODE_Z = 29,

    SDL_SCANCODE_1 = 30,
    SDL_SCANCODE_2 = 31,
    SDL_SCANCODE_3 = 32,
    SDL_SCANCODE_4 = 33,
    SDL_SCANCODE_5 = 34,
    SDL_SCANCODE_6 = 35,
    SDL_SCANCODE_7 = 36,
    SDL_SCANCODE_8 = 37,
    SDL_SCANCODE_9 = 38,
    SDL_SCANCODE_0 = 39,

    SDL_SCANCODE_RETURN = 40,
    SDL_SCANCODE_ESCAPE = 41,
    SDL_SCANCODE_BACKSPACE = 42,
    SDL_SCANCODE_TAB = 43,
    SDL_SCANCODE_SPACE = 44,

    SDL_SCANCODE_MINUS = 45,
    SDL_SCANCODE_EQUALS = 46,
    SDL_SCANCODE_LEFTBRACKET = 47,
    SDL_SCANCODE_RIGHTBRACKET = 48,
    SDL_SCANCODE_BACKSLASH = 49,
    SDL_SCANCODE_NONUSHASH = 50,
    SDL_SCANCODE_SEMICOLON = 51,
    SDL_SCANCODE_APOSTROPHE = 52,
    SDL_SCANCODE_GRAVE = 53,
    SDL_SCANCODE_COMMA = 54,
    SDL_SCANCODE_PERIOD = 55,
    SDL_SCANCODE_SLASH = 56,

    SDL_SCANCODE_CAPSLOCK = 57,

    /* The control panel's own hotkeys are F1/F2/F3/F5/F6/F7, and the game
     * reads a whole function row, so the full twelve are kept. */
    SDL_SCANCODE_F1 = 58,
    SDL_SCANCODE_F2 = 59,
    SDL_SCANCODE_F3 = 60,
    SDL_SCANCODE_F4 = 61,
    SDL_SCANCODE_F5 = 62,
    SDL_SCANCODE_F6 = 63,
    SDL_SCANCODE_F7 = 64,
    SDL_SCANCODE_F8 = 65,
    SDL_SCANCODE_F9 = 66,
    SDL_SCANCODE_F10 = 67,
    SDL_SCANCODE_F11 = 68,
    SDL_SCANCODE_F12 = 69,

    SDL_SCANCODE_PRINTSCREEN = 70,
    SDL_SCANCODE_SCROLLLOCK = 71,
    SDL_SCANCODE_PAUSE = 72,
    SDL_SCANCODE_INSERT = 73,
    SDL_SCANCODE_HOME = 74,
    SDL_SCANCODE_PAGEUP = 75,
    SDL_SCANCODE_DELETE = 76,
    SDL_SCANCODE_END = 77,
    SDL_SCANCODE_PAGEDOWN = 78,

    /* Order is RIGHT, LEFT, DOWN, UP in SDL -- not the reading order. */
    SDL_SCANCODE_RIGHT = 79,
    SDL_SCANCODE_LEFT = 80,
    SDL_SCANCODE_DOWN = 81,
    SDL_SCANCODE_UP = 82,

    SDL_SCANCODE_KP_DIVIDE = 84,
    SDL_SCANCODE_KP_MULTIPLY = 85,
    SDL_SCANCODE_KP_MINUS = 86,
    SDL_SCANCODE_KP_PLUS = 87,
    SDL_SCANCODE_KP_ENTER = 88,
    SDL_SCANCODE_KP_1 = 89,
    SDL_SCANCODE_KP_2 = 90,
    SDL_SCANCODE_KP_3 = 91,
    SDL_SCANCODE_KP_4 = 92,
    SDL_SCANCODE_KP_5 = 93,
    SDL_SCANCODE_KP_6 = 94,
    SDL_SCANCODE_KP_7 = 95,
    SDL_SCANCODE_KP_8 = 96,
    SDL_SCANCODE_KP_9 = 97,
    SDL_SCANCODE_KP_0 = 98,
    SDL_SCANCODE_KP_PERIOD = 99,

    SDL_SCANCODE_APPLICATION = 101,

    /* Modifier block: the engine's default bindings are Ctrl (jump) and Alt
     * (throwing), taken from the left-hand variants. */
    SDL_SCANCODE_LCTRL = 224,
    SDL_SCANCODE_LSHIFT = 225,
    SDL_SCANCODE_LALT = 226,
    SDL_SCANCODE_LGUI = 227,
    SDL_SCANCODE_RCTRL = 228,
    SDL_SCANCODE_RSHIFT = 229,
    SDL_SCANCODE_RALT = 230,
    SDL_SCANCODE_RGUI = 231,

    /* Android-style keys upstream's own table maps to Escape. */
    SDL_SCANCODE_SELECT = 119,
    SDL_SCANCODE_AC_BACK = 282,

    /* Not a key: the bound used by be_events_sdl_keymap.h's array length.
     * Must stay the real 512; the keymap header's initialiser list is shorter
     * than this and relies on the remainder being zero-initialised. */
    SDL_SCANCODE_COUNT = 512

} SDL_Scancode;

#endif /* REFKEEN_SDL3_SHIM_SDL_SCANCODE_H */
