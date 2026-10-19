#include "frontend_shell.h"

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

typedef struct fake_core {
    unsigned start;
    unsigned pause;
    unsigned resume;
    unsigned reset;
    unsigned stop;
    unsigned loader;
    retropicoes_menu_result_t next_result;
    size_t last_index;
    retropicoes_rom_item_kind_t last_kind;
} fake_core_t;

static retropicoes_menu_result_t take_result(fake_core_t *core)
{
    retropicoes_menu_result_t result = core->next_result;
    core->next_result = RETROPICOES_MENU_RESULT_OK;
    return result;
}

static retropicoes_menu_result_t start(void *context,
                                       size_t source_index,
                                       retropicoes_rom_item_kind_t item_kind)
{
    fake_core_t *core = (fake_core_t *)context;
    ++core->start;
    core->last_index = source_index;
    core->last_kind = item_kind;
    return take_result(core);
}

#define SIMPLE_CALLBACK(name)                                                  \
    static retropicoes_menu_result_t name(void *context)                       \
    {                                                                          \
        fake_core_t *core = (fake_core_t *)context;                            \
        ++core->name;                                                          \
        return take_result(core);                                              \
    }

SIMPLE_CALLBACK(pause)
SIMPLE_CALLBACK(resume)
SIMPLE_CALLBACK(reset)
SIMPLE_CALLBACK(stop)

static retropicoes_menu_result_t return_loader(void *context)
{
    fake_core_t *core = (fake_core_t *)context;
    ++core->loader;
    return take_result(core);
}

static retropicoes_frontend_lifecycle_t lifecycle(fake_core_t *core)
{
    retropicoes_frontend_lifecycle_t value;
    memset(&value, 0, sizeof(value));
    value.version = RETROPICOES_FRONTEND_LIFECYCLE_VERSION;
    value.struct_size = sizeof(value);
    value.context = core;
    value.start = start;
    value.pause = pause;
    value.resume = resume;
    value.reset = reset;
    value.stop = stop;
    value.return_loader = return_loader;
    return value;
}

static int test_browser_game_settings_resume(void)
{
    fake_core_t core;
    retropicoes_frontend_lifecycle_t ops;
    retropicoes_frontend_shell_t shell;

    memset(&core, 0, sizeof(core));
    ops = lifecycle(&core);
    CHECK(retropicoes_frontend_shell_init(&shell, &ops));
    CHECK(shell.screen == RETROPICOES_FRONTEND_ROM_BROWSER);
    CHECK(retropicoes_frontend_shell_launch(
              &shell, 3u, RETROPICOES_ROM_ITEM_ROM) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(shell.screen == RETROPICOES_FRONTEND_RUNNING);
    CHECK(core.start == 1u && core.last_index == 3u);
    CHECK(retropicoes_frontend_shell_open_settings(&shell) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(shell.screen == RETROPICOES_FRONTEND_SETTINGS && core.pause == 1u);
    CHECK(retropicoes_frontend_shell_settings_action(
              &shell, RETROPICOES_FRONTEND_SETTINGS_RESUME) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(shell.screen == RETROPICOES_FRONTEND_RUNNING && core.resume == 1u);
    return 0;
}

static int test_same_settings_from_browser(void)
{
    fake_core_t core;
    retropicoes_frontend_lifecycle_t ops;
    retropicoes_frontend_shell_t shell;

    memset(&core, 0, sizeof(core));
    ops = lifecycle(&core);
    CHECK(retropicoes_frontend_shell_init(&shell, &ops));
    CHECK(retropicoes_frontend_shell_open_settings(&shell) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(shell.screen == RETROPICOES_FRONTEND_SETTINGS);
    CHECK(core.pause == 0u);
    CHECK(retropicoes_frontend_shell_settings_action(
              &shell, RETROPICOES_FRONTEND_SETTINGS_CANCEL) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(shell.screen == RETROPICOES_FRONTEND_ROM_BROWSER);
    return 0;
}

static int test_rom_browser_and_loader_are_terminal(void)
{
    fake_core_t core;
    retropicoes_frontend_lifecycle_t ops;
    retropicoes_frontend_shell_t shell;

    memset(&core, 0, sizeof(core));
    ops = lifecycle(&core);
    CHECK(retropicoes_frontend_shell_init(&shell, &ops));
    CHECK(retropicoes_frontend_shell_launch(
              &shell, RETROPICOES_ROM_BROWSER_DEMO_INDEX,
              RETROPICOES_ROM_ITEM_DEMO) == RETROPICOES_MENU_RESULT_OK);
    CHECK(retropicoes_frontend_shell_open_settings(&shell) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(retropicoes_frontend_shell_settings_action(
              &shell, RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(shell.screen == RETROPICOES_FRONTEND_ROM_BROWSER);
    CHECK(core.stop == 1u && shell.game_active == 0);

    CHECK(retropicoes_frontend_shell_open_settings(&shell) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(retropicoes_frontend_shell_settings_action(
              &shell, RETROPICOES_FRONTEND_SETTINGS_RETURN_LOADER) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(shell.screen == RETROPICOES_FRONTEND_STOPPED);
    CHECK(core.loader == 1u);
    return 0;
}

static int test_failure_does_not_leave_running_screen(void)
{
    fake_core_t core;
    retropicoes_frontend_lifecycle_t ops;
    retropicoes_frontend_shell_t shell;

    memset(&core, 0, sizeof(core));
    ops = lifecycle(&core);
    CHECK(retropicoes_frontend_shell_init(&shell, &ops));
    CHECK(retropicoes_frontend_shell_launch(
              &shell, 0u, RETROPICOES_ROM_ITEM_ROM) ==
          RETROPICOES_MENU_RESULT_OK);
    core.next_result = RETROPICOES_MENU_RESULT_FAILED;
    CHECK(retropicoes_frontend_shell_open_settings(&shell) ==
          RETROPICOES_MENU_RESULT_FAILED);
    CHECK(shell.screen == RETROPICOES_FRONTEND_RUNNING);
    CHECK(shell.game_active == 1);
    return 0;
}

static int test_reset_from_settings(void)
{
    fake_core_t core;
    retropicoes_frontend_lifecycle_t ops;
    retropicoes_frontend_shell_t shell;

    memset(&core, 0, sizeof(core));
    ops = lifecycle(&core);
    CHECK(retropicoes_frontend_shell_init(&shell, &ops));
    CHECK(retropicoes_frontend_shell_launch(
              &shell, 0u, RETROPICOES_ROM_ITEM_ROM) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(retropicoes_frontend_shell_open_settings(&shell) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(retropicoes_frontend_shell_settings_action(
              &shell, RETROPICOES_FRONTEND_SETTINGS_RESET) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(shell.screen == RETROPICOES_FRONTEND_RUNNING);
    CHECK(core.start == 1u && core.pause == 1u &&
          core.reset == 1u && core.resume == 1u && core.stop == 0u);
    CHECK(retropicoes_frontend_shell_game_finished(&shell) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(core.stop == 1u && shell.screen == RETROPICOES_FRONTEND_ROM_BROWSER);
    return 0;
}

int main(void)
{
    if (test_browser_game_settings_resume() != 0) return 1;
    if (test_same_settings_from_browser() != 0) return 1;
    if (test_rom_browser_and_loader_are_terminal() != 0) return 1;
    if (test_failure_does_not_leave_running_screen() != 0) return 1;
    if (test_reset_from_settings() != 0) return 1;
    puts("common frontend shell tests: PASS");
    return 0;
}
