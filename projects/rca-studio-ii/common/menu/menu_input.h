#ifndef RETROPICOES_COMMON_MENU_INPUT_H
#define RETROPICOES_COMMON_MENU_INPUT_H

#include <stddef.h>
#include <stdint.h>

#include "menu_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RETROPICOES_MENU_NO_CONTROLLER ((size_t)-1)
#define RETROPICOES_MENU_REPEAT_DELAY_MS 500u
#define RETROPICOES_MENU_REPEAT_PERIOD_MS 50u

typedef enum retropicoes_controller_profile {
    RETROPICOES_CONTROLLER_UNKNOWN = 0,
    RETROPICOES_CONTROLLER_NINTENDO,
    RETROPICOES_CONTROLLER_XBOX,
    RETROPICOES_CONTROLLER_PLAYSTATION,
    RETROPICOES_CONTROLLER_GENESIS,
    RETROPICOES_CONTROLLER_SATURN,
    RETROPICOES_CONTROLLER_NES,
    RETROPICOES_CONTROLLER_SNES,
    RETROPICOES_CONTROLLER_KEYBOARD,
    RETROPICOES_CONTROLLER_PROFILE_COUNT
} retropicoes_controller_profile_t;

/* Position-based mask emitted by pico_shared after device normalization.
 * PRIMARY and SECONDARY describe menu position rather than printed labels. */
enum retropicoes_standard_pad_button {
    RETROPICOES_PAD_PRIMARY   = 1u << 0,
    RETROPICOES_PAD_SECONDARY = 1u << 1,
    RETROPICOES_PAD_MENU      = 1u << 2,
    RETROPICOES_PAD_AUX       = 1u << 3,
    RETROPICOES_PAD_UP        = 1u << 4,
    RETROPICOES_PAD_DOWN      = 1u << 5,
    RETROPICOES_PAD_LEFT      = 1u << 6,
    RETROPICOES_PAD_RIGHT     = 1u << 7
};

retropicoes_ui_actions_t
retropicoes_menu_actions_from_standard_pad(uint32_t pad_buttons);

/* Maps the two legacy compat face-button channels to menu semantics.
 * The shared USB layer normalizes Xbox/PlayStation/Sega face buttons, while
 * Nintendo-style USB and native NES/SNES pads retain their printed A/B
 * identity. Use this helper at the menu boundary so physical A confirms and
 * physical B cancels without changing gameplay mappings. */
retropicoes_ui_actions_t
retropicoes_menu_face_actions_from_compat(
    retropicoes_controller_profile_t profile,
    int action_pressed,
    int jump_pressed);

typedef struct retropicoes_controller_labels {
    const char *confirm;
    const char *cancel;
    const char *menu;
    const char *aux;
} retropicoes_controller_labels_t;

const retropicoes_controller_labels_t *
retropicoes_menu_controller_labels(retropicoes_controller_profile_t profile);

/* Classifies the names exposed by pico_shared and USB host backends. */
retropicoes_controller_profile_t
retropicoes_menu_controller_profile_from_name(const char *name);

typedef struct retropicoes_menu_owner {
    size_t active_controller;
} retropicoes_menu_owner_t;

void retropicoes_menu_owner_reset(retropicoes_menu_owner_t *owner);
size_t retropicoes_menu_owner_update(retropicoes_menu_owner_t *owner,
                                     const uint32_t *controller_actions,
                                     size_t controller_count);
void retropicoes_menu_owner_disconnected(retropicoes_menu_owner_t *owner,
                                         size_t controller);

typedef struct retropicoes_menu_repeat {
    uint32_t held_since_ms;
    uint32_t last_repeat_ms;
    retropicoes_ui_actions_t held_direction;
} retropicoes_menu_repeat_t;

void retropicoes_menu_repeat_reset(retropicoes_menu_repeat_t *repeat);

/* Returns an edge or auto-repeat event for exactly one held Up/Down action.
 * Confirm, Cancel and other actions intentionally remain edge-triggered. */
retropicoes_ui_actions_t
retropicoes_menu_repeat_update(retropicoes_menu_repeat_t *repeat,
                               retropicoes_ui_actions_t held_actions,
                               retropicoes_ui_actions_t pressed_actions,
                               uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
