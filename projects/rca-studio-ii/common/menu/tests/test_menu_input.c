#include "menu_input.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
                    #condition);                                               \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static int test_labels(void)
{
    const retropicoes_controller_labels_t *labels;

    labels = retropicoes_menu_controller_labels(
        RETROPICOES_CONTROLLER_NINTENDO);
    CHECK(strcmp(labels->confirm, "A") == 0);
    CHECK(strcmp(labels->cancel, "B") == 0);
    labels = retropicoes_menu_controller_labels(RETROPICOES_CONTROLLER_XBOX);
    CHECK(strcmp(labels->confirm, "A") == 0);
    CHECK(strcmp(labels->cancel, "B") == 0);
    labels = retropicoes_menu_controller_labels(
        RETROPICOES_CONTROLLER_PLAYSTATION);
    CHECK(strcmp(labels->confirm, "O") == 0);
    CHECK(strcmp(labels->cancel, "X") == 0);
    labels = retropicoes_menu_controller_labels(RETROPICOES_CONTROLLER_SATURN);
    CHECK(strcmp(labels->confirm, "A") == 0);
    CHECK(strcmp(labels->cancel, "B") == 0);
    labels = retropicoes_menu_controller_labels(
        (retropicoes_controller_profile_t)999);
    CHECK(strcmp(labels->confirm, "A") == 0);
    CHECK(strcmp(labels->cancel, "B") == 0);
    CHECK(retropicoes_menu_controller_profile_from_name("Xbox 360 Wired") ==
          RETROPICOES_CONTROLLER_XBOX);
    CHECK(retropicoes_menu_controller_profile_from_name("Dual Sense") ==
          RETROPICOES_CONTROLLER_PLAYSTATION);
    CHECK(retropicoes_menu_controller_profile_from_name("HORI Switch Pad") ==
          RETROPICOES_CONTROLLER_NINTENDO);
    CHECK(retropicoes_menu_controller_profile_from_name("Saturn USB") ==
          RETROPICOES_CONTROLLER_SATURN);
    CHECK(retropicoes_menu_controller_profile_from_name("unknown pad") ==
          RETROPICOES_CONTROLLER_UNKNOWN);
    return 0;
}

static int test_compat_face_mapping(void)
{
    retropicoes_ui_actions_t actions;

    /* With ABSWAPPED=1, XInput's physical B/A arrive on ACTION/JUMP. */
    actions = retropicoes_menu_face_actions_from_compat(
        RETROPICOES_CONTROLLER_XBOX, 1, 0);
    CHECK(actions == RETROPICOES_UI_CANCEL);
    actions = retropicoes_menu_face_actions_from_compat(
        RETROPICOES_CONTROLLER_XBOX, 0, 1);
    CHECK(actions == RETROPICOES_UI_CONFIRM);

    /* HORI/Nintendo and native NES/SNES retain printed A on ACTION, B on JUMP. */
    actions = retropicoes_menu_face_actions_from_compat(
        RETROPICOES_CONTROLLER_NINTENDO, 0, 1);
    CHECK(actions == RETROPICOES_UI_CANCEL);
    actions = retropicoes_menu_face_actions_from_compat(
        RETROPICOES_CONTROLLER_NINTENDO, 1, 0);
    CHECK(actions == RETROPICOES_UI_CONFIRM);
    actions = retropicoes_menu_face_actions_from_compat(
        RETROPICOES_CONTROLLER_NES, 0, 1);
    CHECK(actions == RETROPICOES_UI_CANCEL);
    actions = retropicoes_menu_face_actions_from_compat(
        RETROPICOES_CONTROLLER_SNES, 1, 0);
    CHECK(actions == RETROPICOES_UI_CONFIRM);
    return 0;
}

static int test_owner_lock(void)
{
    retropicoes_menu_owner_t owner;
    uint32_t actions[2] = {0u, 0u};

    retropicoes_menu_owner_reset(&owner);
    CHECK(retropicoes_menu_owner_update(&owner, actions, 2u) ==
          RETROPICOES_MENU_NO_CONTROLLER);
    actions[1] = 1u;
    CHECK(retropicoes_menu_owner_update(&owner, actions, 2u) == 1u);
    actions[0] = 1u;
    actions[1] = 0u;
    CHECK(retropicoes_menu_owner_update(&owner, actions, 2u) == 1u);
    retropicoes_menu_owner_disconnected(&owner, 1u);
    CHECK(retropicoes_menu_owner_update(&owner, actions, 2u) == 0u);
    return 0;
}

static int test_standard_pad_mapping(void)
{
    retropicoes_ui_actions_t actions =
        retropicoes_menu_actions_from_standard_pad(
            RETROPICOES_PAD_UP | RETROPICOES_PAD_RIGHT |
            RETROPICOES_PAD_PRIMARY | RETROPICOES_PAD_SECONDARY |
            RETROPICOES_PAD_MENU | RETROPICOES_PAD_AUX);
    CHECK((actions & RETROPICOES_UI_UP) != 0u);
    CHECK((actions & RETROPICOES_UI_RIGHT) != 0u);
    CHECK((actions & RETROPICOES_UI_CONFIRM) != 0u);
    CHECK((actions & RETROPICOES_UI_CANCEL) != 0u);
    CHECK((actions & RETROPICOES_UI_MENU) != 0u);
    CHECK((actions & RETROPICOES_UI_AUX) != 0u);
    CHECK((actions & (RETROPICOES_UI_DOWN | RETROPICOES_UI_LEFT)) == 0u);
    return 0;
}

static int test_vertical_auto_repeat(void)
{
    retropicoes_menu_repeat_t repeat;
    retropicoes_ui_actions_t result;

    retropicoes_menu_repeat_reset(&repeat);
    result = retropicoes_menu_repeat_update(
        &repeat, RETROPICOES_UI_DOWN, RETROPICOES_UI_DOWN, 100u);
    CHECK(result == RETROPICOES_UI_DOWN);
    result = retropicoes_menu_repeat_update(
        &repeat, RETROPICOES_UI_DOWN, 0u, 599u);
    CHECK(result == 0u);
    result = retropicoes_menu_repeat_update(
        &repeat, RETROPICOES_UI_DOWN, 0u, 600u);
    CHECK(result == RETROPICOES_UI_DOWN);
    result = retropicoes_menu_repeat_update(
        &repeat, RETROPICOES_UI_DOWN, 0u, 649u);
    CHECK(result == 0u);
    result = retropicoes_menu_repeat_update(
        &repeat, RETROPICOES_UI_DOWN, 0u, 650u);
    CHECK(result == RETROPICOES_UI_DOWN);

    result = retropicoes_menu_repeat_update(
        &repeat, RETROPICOES_UI_UP, RETROPICOES_UI_UP, 660u);
    CHECK(result == RETROPICOES_UI_UP);
    result = retropicoes_menu_repeat_update(
        &repeat, RETROPICOES_UI_UP | RETROPICOES_UI_DOWN,
        RETROPICOES_UI_DOWN, 700u);
    CHECK(result == 0u);
    result = retropicoes_menu_repeat_update(
        &repeat, 0u, 0u, 701u);
    CHECK(result == 0u);
    return 0;
}

int main(void)
{
    CHECK(test_labels() == 0);
    CHECK(test_compat_face_mapping() == 0);
    CHECK(test_owner_lock() == 0);
    CHECK(test_standard_pad_mapping() == 0);
    CHECK(test_vertical_auto_repeat() == 0);
    puts("common menu input tests: PASS");
    return 0;
}
