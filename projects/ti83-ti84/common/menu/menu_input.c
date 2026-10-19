#include "menu_input.h"

#include <string.h>
#include <ctype.h>

static const retropicoes_controller_labels_t controller_labels[] = {
    {"A", "B", "MENU", "AUX"},
    {"A", "B", "SELECT", "X"},
    {"A", "B", "BACK", "Y"},
    {"O", "X", "SELECT", "TRIANGLE"},
    {"A", "B", "MODE", "C"},
    {"A", "B", "MODE", "C"},
    {"A", "B", "SELECT", ""},
    {"A", "B", "SELECT", "X"},
    {"X", "Z", "TAB", "C"}
};

retropicoes_ui_actions_t
retropicoes_menu_actions_from_standard_pad(uint32_t pad_buttons)
{
    retropicoes_ui_actions_t actions = 0u;
    if ((pad_buttons & RETROPICOES_PAD_UP) != 0u)
        actions |= RETROPICOES_UI_UP;
    if ((pad_buttons & RETROPICOES_PAD_DOWN) != 0u)
        actions |= RETROPICOES_UI_DOWN;
    if ((pad_buttons & RETROPICOES_PAD_LEFT) != 0u)
        actions |= RETROPICOES_UI_LEFT;
    if ((pad_buttons & RETROPICOES_PAD_RIGHT) != 0u)
        actions |= RETROPICOES_UI_RIGHT;
    if ((pad_buttons & RETROPICOES_PAD_PRIMARY) != 0u)
        actions |= RETROPICOES_UI_CONFIRM;
    if ((pad_buttons & RETROPICOES_PAD_SECONDARY) != 0u)
        actions |= RETROPICOES_UI_CANCEL;
    if ((pad_buttons & RETROPICOES_PAD_MENU) != 0u)
        actions |= RETROPICOES_UI_MENU;
    if ((pad_buttons & RETROPICOES_PAD_AUX) != 0u)
        actions |= RETROPICOES_UI_AUX;
    return actions;
}

retropicoes_ui_actions_t
retropicoes_menu_face_actions_from_compat(
    retropicoes_controller_profile_t profile,
    int action_pressed,
    int jump_pressed)
{
    const int jump_is_confirm =
        profile == RETROPICOES_CONTROLLER_XBOX ||
        profile == RETROPICOES_CONTROLLER_GENESIS ||
        profile == RETROPICOES_CONTROLLER_SATURN;
    retropicoes_ui_actions_t actions = 0u;

    if (action_pressed)
        actions |= jump_is_confirm ? RETROPICOES_UI_CANCEL
                                   : RETROPICOES_UI_CONFIRM;
    if (jump_pressed)
        actions |= jump_is_confirm ? RETROPICOES_UI_CONFIRM
                                   : RETROPICOES_UI_CANCEL;
    return actions;
}

_Static_assert(sizeof(controller_labels) / sizeof(controller_labels[0]) ==
                   RETROPICOES_CONTROLLER_PROFILE_COUNT,
               "controller label table must cover every profile");

const retropicoes_controller_labels_t *
retropicoes_menu_controller_labels(retropicoes_controller_profile_t profile)
{
    if ((unsigned)profile >= (unsigned)RETROPICOES_CONTROLLER_PROFILE_COUNT) {
        profile = RETROPICOES_CONTROLLER_UNKNOWN;
    }
    return &controller_labels[(unsigned)profile];
}

static int contains_case_insensitive(const char *text, const char *needle)
{
    size_t needle_length;
    const char *start;
    if (text == NULL || needle == NULL || *needle == '\0') return 0;
    needle_length = strlen(needle);
    for (start = text; *start != '\0'; ++start) {
        size_t i;
        for (i = 0u; i < needle_length; ++i) {
            if (start[i] == '\0' ||
                tolower((unsigned char)start[i]) !=
                    tolower((unsigned char)needle[i]))
                break;
        }
        if (i == needle_length) return 1;
    }
    return 0;
}

retropicoes_controller_profile_t
retropicoes_menu_controller_profile_from_name(const char *name)
{
    if (name == NULL || *name == '\0') return RETROPICOES_CONTROLLER_UNKNOWN;
    if (contains_case_insensitive(name, "dual shock") ||
        contains_case_insensitive(name, "dualsense") ||
        contains_case_insensitive(name, "dual sense") ||
        contains_case_insensitive(name, "playstation") ||
        contains_case_insensitive(name, "psclassic"))
        return RETROPICOES_CONTROLLER_PLAYSTATION;
    if (contains_case_insensitive(name, "xinput") ||
        contains_case_insensitive(name, "xbox"))
        return RETROPICOES_CONTROLLER_XBOX;
    if (contains_case_insensitive(name, "saturn"))
        return RETROPICOES_CONTROLLER_SATURN;
    if (contains_case_insensitive(name, "genesis") ||
        contains_case_insensitive(name, "megadrive") ||
        contains_case_insensitive(name, "mdarcade"))
        return RETROPICOES_CONTROLLER_GENESIS;
    if (contains_case_insensitive(name, "snes"))
        return RETROPICOES_CONTROLLER_SNES;
    if (contains_case_insensitive(name, "nes"))
        return RETROPICOES_CONTROLLER_NES;
    if (contains_case_insensitive(name, "nintendo") ||
        contains_case_insensitive(name, "switch") ||
        contains_case_insensitive(name, "hori"))
        return RETROPICOES_CONTROLLER_NINTENDO;
    if (contains_case_insensitive(name, "keyboard"))
        return RETROPICOES_CONTROLLER_KEYBOARD;
    return RETROPICOES_CONTROLLER_UNKNOWN;
}

void retropicoes_menu_owner_reset(retropicoes_menu_owner_t *owner)
{
    if (owner != NULL) {
        owner->active_controller = RETROPICOES_MENU_NO_CONTROLLER;
    }
}

size_t retropicoes_menu_owner_update(retropicoes_menu_owner_t *owner,
                                     const uint32_t *controller_actions,
                                     size_t controller_count)
{
    size_t i;

    if (owner == NULL) {
        return RETROPICOES_MENU_NO_CONTROLLER;
    }
    if (owner->active_controller < controller_count) {
        return owner->active_controller;
    }
    if (controller_actions == NULL) {
        return RETROPICOES_MENU_NO_CONTROLLER;
    }
    for (i = 0; i < controller_count; ++i) {
        if (controller_actions[i] != 0u) {
            owner->active_controller = i;
            return i;
        }
    }
    owner->active_controller = RETROPICOES_MENU_NO_CONTROLLER;
    return RETROPICOES_MENU_NO_CONTROLLER;
}

void retropicoes_menu_owner_disconnected(retropicoes_menu_owner_t *owner,
                                         size_t controller)
{
    if (owner != NULL && owner->active_controller == controller) {
        owner->active_controller = RETROPICOES_MENU_NO_CONTROLLER;
    }
}

static int elapsed_at_least(uint32_t now, uint32_t since, uint32_t duration)
{
    return (uint32_t)(now - since) >= duration;
}

void retropicoes_menu_repeat_reset(retropicoes_menu_repeat_t *repeat)
{
    if (repeat != NULL) memset(repeat, 0, sizeof(*repeat));
}

retropicoes_ui_actions_t
retropicoes_menu_repeat_update(retropicoes_menu_repeat_t *repeat,
                               retropicoes_ui_actions_t held_actions,
                               retropicoes_ui_actions_t pressed_actions,
                               uint32_t now_ms)
{
    const retropicoes_ui_actions_t vertical =
        held_actions & (RETROPICOES_UI_UP | RETROPICOES_UI_DOWN);
    retropicoes_ui_actions_t pressed_vertical =
        pressed_actions & (RETROPICOES_UI_UP | RETROPICOES_UI_DOWN);

    if (repeat == NULL) return pressed_vertical;

    /* Opposing directions are neutral.  A newly selected single direction
     * moves immediately, then follows the Loader's delayed repeat cadence. */
    if (vertical == 0u ||
        vertical == (RETROPICOES_UI_UP | RETROPICOES_UI_DOWN)) {
        retropicoes_menu_repeat_reset(repeat);
        return 0u;
    }
    if (repeat->held_direction != vertical) {
        repeat->held_direction = vertical;
        repeat->held_since_ms = now_ms;
        repeat->last_repeat_ms = now_ms;
        return vertical;
    }
    if (!elapsed_at_least(now_ms, repeat->held_since_ms,
                          RETROPICOES_MENU_REPEAT_DELAY_MS)) {
        return 0u;
    }
    if (elapsed_at_least(now_ms, repeat->last_repeat_ms,
                         RETROPICOES_MENU_REPEAT_PERIOD_MS)) {
        repeat->last_repeat_ms = now_ms;
        return vertical;
    }
    return 0u;
}
