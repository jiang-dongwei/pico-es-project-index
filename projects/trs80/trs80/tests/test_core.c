/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>

#include "trs80.h"
#include "contract.h"
#include "virtual_keyboard.h"

static unsigned failures;
#define CHECK(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); ++failures; \
} } while (0)

static size_t empty_rom_count(void *context)
{
    (void)context;
    return 0u;
}

static int empty_rom_item(void *context, size_t index,
                          retropicoes_rom_item_view_t *item)
{
    (void)context; (void)index; (void)item;
    return 0;
}

static retropicoes_menu_result_t frontend_start(
    void *context, size_t index, retropicoes_rom_item_kind_t kind)
{
    (void)context; (void)index; (void)kind;
    return RETROPICOES_MENU_RESULT_OK;
}

static retropicoes_menu_result_t frontend_noop(void *context)
{
    (void)context;
    return RETROPICOES_MENU_RESULT_OK;
}

int main(void)
{
    CHECK(RETROPICOES_FRONTEND_CONTRACT_VERSION == 2u);
    char path[96];
    trs80_menu_context_t menu_context = {
        TRS80_MENU_REQUEST_NONE
    };
    retropicoes_menu_ops_t menu_ops;
    retropicoes_frontend_shell_t shell;
    retropicoes_rom_browser_t browser;
    retropicoes_frontend_lifecycle_t lifecycle = {
        RETROPICOES_FRONTEND_LIFECYCLE_VERSION,
        sizeof(retropicoes_frontend_lifecycle_t), NULL,
        frontend_start, frontend_noop, frontend_noop,
        frontend_noop, frontend_noop, frontend_noop
    };
    retropicoes_rom_source_t source = {
        RETROPICOES_ROM_SOURCE_VERSION,
        sizeof(retropicoes_rom_source_t), NULL,
        "TRS-80 Model I / III / 4", "No ROMs", "Demo",
        empty_rom_count, empty_rom_item, NULL
    };


    CHECK((trs80_menu_actions(TRS80_KEY_CONFIRM) &
           RETROPICOES_UI_CONFIRM) != 0u);
    CHECK((trs80_menu_actions(TRS80_KEY_CANCEL) &
           RETROPICOES_UI_CANCEL) != 0u);
    trs80_menu_build_ops(&menu_ops, &menu_context);
    CHECK(retropicoes_menu_contract_valid(&menu_ops));
    CHECK((menu_ops.capabilities & RETROPICOES_MENU_CAP_STATE) == 0u);
    CHECK(retropicoes_menu_invoke(
              &menu_ops, RETROPICOES_MENU_ITEM_RETURN_LOADER) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(menu_context.request ==
          TRS80_MENU_REQUEST_RETURN_LOADER);

    CHECK(trs80_frontend_init(
        &shell, &browser, &lifecycle, &source));
    CHECK(shell.screen == RETROPICOES_FRONTEND_ROM_BROWSER);
    CHECK(browser.showing_demo);
    CHECK(retropicoes_rom_browser_update(
        &browser, RETROPICOES_UI_CONFIRM).type ==
        RETROPICOES_ROM_BROWSER_EVENT_OPEN);

    CHECK(trs80_make_rom_path(path, sizeof(path), "game.rom"));
    CHECK(strcmp(path, "/roms/TRS80/game.rom") == 0);
    CHECK(trs80_make_save_path(path, sizeof(path), "slot1.sav"));
    CHECK(strcmp(path, "/SAVES/TRS80/slot1.sav") == 0);
    CHECK(!trs80_make_rom_path(path, 4u, "game.rom"));
    CHECK(!trs80_make_rom_path(path, sizeof(path), "/escape"));
    CHECK(!trs80_make_rom_path(path, sizeof(path), "../escape"));
    CHECK(!trs80_make_save_path(path, sizeof(path), "..\\escape"));
    CHECK(!trs80_make_save_path(path, sizeof(path), "C:escape"));
    CHECK(!trs80_make_save_path(path, sizeof(path), ".."));

    trs80_virtual_keyboard_t keyboard;
    trs80_vk_open(&keyboard, TRS80_MODEL_III);
    CHECK(keyboard.selected == TRS80_MATRIX_ENTER);
    CHECK(trs80_vk_update(&keyboard, RETROPICOES_UI_CONFIRM).type == TRS80_VK_NONE);
    (void)trs80_vk_update(&keyboard, 0u);
    trs80_vk_result_t event = trs80_vk_update(&keyboard, RETROPICOES_UI_CONFIRM);
    CHECK(event.type == TRS80_VK_SEND && event.key == TRS80_MATRIX_ENTER && !event.shift);
    CHECK(trs80_vk_update(&keyboard, RETROPICOES_UI_CONFIRM).type == TRS80_VK_NONE);
    (void)trs80_vk_update(&keyboard, 0u);
    (void)trs80_vk_update(&keyboard, RETROPICOES_UI_DOWN);
    CHECK(keyboard.selected == TRS80_MATRIX_SHIFT);
    (void)trs80_vk_update(&keyboard, 0u);
    (void)trs80_vk_update(&keyboard, RETROPICOES_UI_CONFIRM);
    CHECK(keyboard.shift);
    (void)trs80_vk_update(&keyboard, 0u);
    (void)trs80_vk_update(&keyboard, RETROPICOES_UI_UP);
    (void)trs80_vk_update(&keyboard, 0u);
    event = trs80_vk_update(&keyboard, RETROPICOES_UI_CONFIRM);
    CHECK(event.type == TRS80_VK_SEND && event.shift);
    (void)trs80_vk_update(&keyboard, 0u);
    CHECK(trs80_vk_update(&keyboard, RETROPICOES_UI_CANCEL).type == TRS80_VK_CLOSE);
    CHECK(!keyboard.open && !keyboard.shift);
    CHECK(trs80_vk_label(TRS80_MODEL_I, 57u) == NULL);
    CHECK(trs80_vk_label(TRS80_MODEL_III, 58u) == NULL);
    CHECK(strcmp(trs80_vk_label(TRS80_MODEL_4, 58u), "CTRL") == 0);
    trs80_vk_open(&keyboard, TRS80_MODEL_I);
    (void)trs80_vk_update(&keyboard, 0u);
    for (unsigned direction = RETROPICOES_UI_UP; direction <= RETROPICOES_UI_RIGHT;
         direction <<= 1u) {
        for (unsigned i = 0u; i < 80u; ++i) {
            (void)trs80_vk_update(&keyboard, direction);
            CHECK(trs80_vk_label(keyboard.model, keyboard.selected) != NULL);
            (void)trs80_vk_update(&keyboard, 0u);
        }
    }
    static uint16_t pixels[320u * 240u + 2u];
    pixels[0] = pixels[320u * 240u + 1u] = 0x1234u;
    retropicoes_menu_surface_t surface = {pixels + 1u, 320u, 320u, 240u};
    trs80_vk_render(&keyboard, &surface,
        retropicoes_menu_controller_labels(RETROPICOES_CONTROLLER_XBOX));
    CHECK(pixels[0] == 0x1234u && pixels[320u * 240u + 1u] == 0x1234u);

    if (failures != 0u) return 1;
    puts("TRS-80 PUBLIC FRONTEND CONTRACT: PASS");
    return 0;
}
