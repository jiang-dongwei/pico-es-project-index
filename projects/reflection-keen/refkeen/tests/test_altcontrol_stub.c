/* Host test for the port's altcontrol stub (S20-C / C12).
 *
 * WHAT THIS CAN AND CANNOT REACH
 * ------------------------------
 * The stub is link-time binding far more than it is logic: the interesting
 * questions are "does every symbol the engine asks for exist here", "is a
 * zero-filled mapping really an empty mapping", and "can the current mapping
 * ever be one of the two identity markers". Those are all answerable on the
 * host, and answering them here means a missing symbol shows up as a link error
 * in a five-second test instead of as an undefined reference in C13's firmware
 * link.
 *
 * What it cannot reach is the rest of the engine: this file links against the
 * stub alone, so it cannot prove that the stub's 32 definitions are exactly the
 * 32 the built TUs ask for. That is the gate's job --
 * Test-AltControlStubContract.ps1 compiles all 62 TUs of the Keen Dreams target
 * and intersects their undefined symbols with the two replaced TUs'
 * definitions.
 *
 * The two premises the stub leans on are pinned as compile-time checks rather
 * than left as prose:
 *
 *   * BE_ST_CTRL_MAP_NONE is 0, so a zero-initialised mapping is an empty
 *     mapping. If upstream ever renumbers that enum, the whole stub is wrong.
 *   * BE_MAX_KEY_ID is 250, which is what makes the mappings 6 KB each and the
 *     stub's RAM cost worth recording.
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "altcontrol_stub.h"

/* A zero-initialised BE_ST_ControllerMapping is valid only because
 * BE_ST_CTRL_MAP_NONE is 0 (be_st.h:178). C99-compatible static assert. */
typedef char stub_assert_none_is_zero[(BE_ST_CTRL_MAP_NONE == 0) ? 1 : -1];
typedef char stub_assert_key_bound[(BE_MAX_KEY_ID == 250) ? 1 : -1];

/* Every symbol the port has to provide, as the port provides it. Taking the
 * address is the point: an undeclared name fails to compile here, and a
 * declared-but-undefined one fails to link. The name is only for the report. */
struct stub_symbol {
    const char *name;
    const void *address;
};

static const struct stub_symbol g_symbols[] = {
    /* game half -- replaces src/kdreams/altcontroller.c */
    { "g_binding_value_button", &g_binding_value_button },
    { "g_binding_value_stats", &g_binding_value_stats },
    { "g_binding_value_up", &g_binding_value_up },
    { "g_binding_value_down", &g_binding_value_down },
    { "g_binding_value_left", &g_binding_value_left },
    { "g_binding_value_right", &g_binding_value_right },
    { "g_keybind_used_button", &g_keybind_used_button },
    { "g_keybind_used_stats", &g_keybind_used_stats },
    { "g_keybind_used_up", &g_keybind_used_up },
    { "g_keybind_used_down", &g_keybind_used_down },
    { "g_keybind_used_left", &g_keybind_used_left },
    { "g_keybind_used_right", &g_keybind_used_right },
    { "g_ingame_altcontrol_mapping_notenoughmemorytostart",
      &g_ingame_altcontrol_mapping_notenoughmemorytostart },
    { "g_ingame_altcontrol_mapping_inackback",
      &g_ingame_altcontrol_mapping_inackback },
    { "g_ingame_altcontrol_mapping_simpledialog",
      &g_ingame_altcontrol_mapping_simpledialog },
    { "g_ingame_altcontrol_mapping_demoloop",
      &g_ingame_altcontrol_mapping_demoloop },
    { "g_ingame_altcontrol_mapping_menu", &g_ingame_altcontrol_mapping_menu },
    { "g_ingame_altcontrol_mapping_menu_help",
      &g_ingame_altcontrol_mapping_menu_help },
    { "RefKeen_PrepareAltControllerScheme",
      (const void *)&RefKeen_PrepareAltControllerScheme },
    { "FinalizeControlPanelMappingsByMousePresence",
      (const void *)&FinalizeControlPanelMappingsByMousePresence },
    { "UpdateGameplayMappingsByMousePresence",
      (const void *)&UpdateGameplayMappingsByMousePresence },
    { "PrepareGamePlayControllerMapping",
      (const void *)&PrepareGamePlayControllerMapping },

    /* backend half -- replaces be_input_controller_mappings.c */
    { "g_sdlControllerMappingActualCurr", &g_sdlControllerMappingActualCurr },
    { "g_beStControllerMappingTextInput", &g_beStControllerMappingTextInput },
    { "g_beStControllerMappingDebugKeys", &g_beStControllerMappingDebugKeys },
    { "BEL_ST_AltControlScheme_ClearBinaryStates",
      (const void *)&BEL_ST_AltControlScheme_ClearBinaryStates },
    { "BEL_ST_AltControlScheme_HandleEntry",
      (const void *)&BEL_ST_AltControlScheme_HandleEntry },
    { "BE_ST_AltControlScheme_Push", (const void *)&BE_ST_AltControlScheme_Push },
    { "BE_ST_AltControlScheme_Pop", (const void *)&BE_ST_AltControlScheme_Pop },
    { "BE_ST_AltControlScheme_Reset", (const void *)&BE_ST_AltControlScheme_Reset },
    { "BE_ST_AltControlScheme_PrepareControllerMapping",
      (const void *)&BE_ST_AltControlScheme_PrepareControllerMapping },
    { "BE_ST_AltControlScheme_UpdateVirtualMouseCursor",
      (const void *)&BE_ST_AltControlScheme_UpdateVirtualMouseCursor },
};

#define STUB_SYMBOL_COUNT (sizeof(g_symbols) / sizeof(g_symbols[0]))

/* --- the contract, as behaviour ------------------------------------------- */

static void test_every_symbol_has_an_address(void)
{
    size_t i;
    for (i = 0; i < STUB_SYMBOL_COUNT; ++i) {
        if (g_symbols[i].address == NULL) {
            printf("FAIL missing symbol: %s\n", g_symbols[i].name);
            assert(0);
        }
    }
    /* A duplicate here would mean two names collapsed onto one object, which is
     * the aliasing design that was considered and rejected (altcontrol_stub.c):
     * it would be invisible at runtime but wrong as a record of the tree. */
    {
        size_t j;
        for (i = 0; i < STUB_SYMBOL_COUNT; ++i) {
            for (j = i + 1; j < STUB_SYMBOL_COUNT; ++j) {
                if (g_symbols[i].address == g_symbols[j].address) {
                    printf("FAIL %s and %s share an address\n",
                           g_symbols[i].name, g_symbols[j].name);
                    assert(0);
                }
            }
        }
    }
}

static void test_a_mapping_is_current_before_anything_runs(void)
{
    /* be_video_ui.c dereferences this pointer without a null check, and
     * be_startup.c's Reset() call is later than the first such lookup. So the
     * only acceptable state at the start of main() is a valid mapping. */
    assert(g_sdlControllerMappingActualCurr != NULL);
    assert(g_sdlControllerMappingActualCurr->keys[0].map.mapClass ==
           BE_ST_CTRL_MAP_NONE);
    assert(g_sdlControllerMappingActualCurr->defaultMapping.mapClass ==
           BE_ST_CTRL_MAP_NONE);
}

static void test_the_current_mapping_is_never_an_identity_marker(void)
{
    /* The documented deviation: no alternative scheme is reachable, so the
     * on-screen keyboard and debug-keys UI stay off. */
    const BE_ST_ControllerMapping *const before = g_sdlControllerMappingActualCurr;

    BE_ST_AltControlScheme_Push();
    assert(g_sdlControllerMappingActualCurr == before);
    BE_ST_AltControlScheme_PrepareControllerMapping(&g_beStControllerMappingTextInput);
    assert(g_sdlControllerMappingActualCurr != &g_beStControllerMappingTextInput);
    BE_ST_AltControlScheme_Pop();
    assert(g_sdlControllerMappingActualCurr == before);

    BE_ST_AltControlScheme_PrepareControllerMapping(
        &g_ingame_altcontrol_mapping_menu);
    assert(g_sdlControllerMappingActualCurr == before);

    assert(g_sdlControllerMappingActualCurr != &g_beStControllerMappingDebugKeys);
    assert(&g_beStControllerMappingTextInput != &g_beStControllerMappingDebugKeys);

    BE_ST_AltControlScheme_Reset();
    assert(g_sdlControllerMappingActualCurr == before);
}

static void test_handle_entry_is_inert_on_an_empty_mapping(void)
{
    /* Upstream's HandleAnyEntry() returns false for BE_ST_CTRL_MAP_NONE before it
     * assigns the new state, so the stub leaving the state alone is not a
     * shortcut -- it is the same observable behaviour. */
    int32_t state = 0x1234;
    assert(!BEL_ST_AltControlScheme_HandleEntry(
        &g_sdlControllerMappingActualCurr->defaultMapping, 32000, &state));
    assert(state == 0x1234);

    assert(!BEL_ST_AltControlScheme_HandleEntry(
        &g_ingame_altcontrol_mapping_menu.mbuttons[0], 32000, &state));
    assert(state == 0x1234);

    /* And the entry points that would act on a real mapping must not touch
     * anything either. */
    BEL_ST_AltControlScheme_ClearBinaryStates();
    BE_ST_AltControlScheme_UpdateVirtualMouseCursor(160, 100);
}

static void test_the_kept_state_starts_zeroed(void)
{
    /* These are read by live game code paths (id_in.c, kd_play.c), so they must
     * be real storage with upstream's initial values, not absent. */
    unsigned i;
    for (i = 0; i < 2u; ++i) {
        assert(g_binding_value_button[i] == 0);
        assert(g_keybind_used_button[i] == false);
    }
    assert(g_binding_value_stats == 0 && g_keybind_used_stats == false);
    assert(g_binding_value_up == 0 && g_keybind_used_up == false);
    assert(g_binding_value_down == 0 && g_keybind_used_down == false);
    assert(g_binding_value_left == 0 && g_keybind_used_left == false);
    assert(g_binding_value_right == 0 && g_keybind_used_right == false);
}

static void test_the_game_side_no_ops_are_callable(void)
{
    /* All four are called by TUs the port builds; each must accept its argument
     * and return. */
    RefKeen_PrepareAltControllerScheme();
    PrepareGamePlayControllerMapping();
    FinalizeControlPanelMappingsByMousePresence(true);
    FinalizeControlPanelMappingsByMousePresence(false);
    UpdateGameplayMappingsByMousePresence(true);
    UpdateGameplayMappingsByMousePresence(false);
}

int main(void)
{
    test_every_symbol_has_an_address();
    test_a_mapping_is_current_before_anything_runs();
    test_the_current_mapping_is_never_an_identity_marker();
    test_handle_entry_is_inert_on_an_empty_mapping();
    test_the_kept_state_starts_zeroed();
    test_the_game_side_no_ops_are_callable();

    /* Facts the gate re-derives from the target objects, printed from the
     * compiled stub so the two must agree. */
    printf("CONTRACT symbols=%d\n", (int)STUB_SYMBOL_COUNT);
    printf("CONTRACT sizeof_mapping=%d\n", (int)sizeof(BE_ST_ControllerMapping));
    printf("CONTRACT max_key_id=%d\n", (int)BE_MAX_KEY_ID);
    {
        size_t i;
        for (i = 0; i < STUB_SYMBOL_COUNT; ++i) {
            printf("CONTRACT symbol=%s\n", g_symbols[i].name);
        }
    }

    puts("REFKEEN ALTCONTROL STUB: PASS");
    return 0;
}
