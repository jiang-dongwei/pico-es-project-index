/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <cstdio>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#include "perf_report.h"
#pragma GCC diagnostic pop
#include <cstring>
#include <cstdlib>

extern "C" {
#include "compat.h"
#include "frontend.h"
#include "frontend_shell.h"
#include "adapter.h"
#include "program.h"
#include "input_backend.h"
#include "input_mapping.h"
#include "menu_adapter.h"
#include "menu_input.h"
#include "menu_render.h"
#include "rom_browser.h"
#include "menu_runtime.h"
#include "menu_state.h"
#include "settings_menu.h"

#include "runtime.h"
#include "settings_ui.h"
}

namespace {
constexpr unsigned kMaxRoms = 64;
constexpr unsigned kNameCapacity = 96;
constexpr uint32_t kFrameBudgetUs = 16667u;
constexpr uint32_t kDelayGuardUs = 500u;

struct RomEntry {
    char name[kNameCapacity];
    char display[kNameCapacity + 2u];
    bool is_directory;
};

RomEntry g_roms[kMaxRoms];
retropicoes_menu_owner_t g_menu_owner{
    RETROPICOES_MENU_NO_CONTROLLER
};
retropicoes_controller_profile_t g_menu_profile =
    RETROPICOES_CONTROLLER_UNKNOWN;

void wait_for_frame_deadline(uint64_t deadline_us)
{
    for (;;) {
        const uint64_t now_us = compat_ticks_us();
        if (now_us >= deadline_us) return;

        const uint64_t remaining_us = deadline_us - now_us;
        if (remaining_us > 1000u + kDelayGuardUs) {
            const uint32_t delay_ms = static_cast<uint32_t>(
                (remaining_us - kDelayGuardUs) / 1000u);
            if (delay_ms != 0u) compat_delay_ms(delay_ms);
        }
    }
}

retropicoes_menu_renderer_t rom_browser_renderer()
{
    retropicoes_menu_renderer_t renderer =
        retropicoes_menu_nintendo_renderer();
    const auto *settings = retropicoes_ti83_settings_ui_current();
    const uint16_t fg = retropicoes_ti83_theme_color(
        settings->foreground_color);
    const uint16_t bg = retropicoes_ti83_theme_color(
        settings->background_color);
    renderer.theme = {bg, fg, fg, fg, fg, bg, 0xf800u};
    return renderer;
}

retropicoes_menu_surface_t menu_surface(compat_framebuffer_t *fb)
{
    return {fb->pixels, fb->stride, fb->width, fb->height};
}

uint32_t pressed_compat_mask()
{
    uint32_t result = 0u;
    for (unsigned key = 0; key < COMPAT_KEY_COUNT; ++key)
        if (compat_key_pressed(static_cast<compat_key_t>(key)))
            result |= 1u << key;
    if (retropicoes_input_menu_pressed()) result |= 1u << COMPAT_KEY_BACK;
    return result;
}

uint32_t down_compat_mask()
{
    uint32_t result = 0u;
    for (unsigned key = 0; key < COMPAT_KEY_COUNT; ++key)
        if (compat_key_down(static_cast<compat_key_t>(key)))
            result |= 1u << key;
    return result;
}

retropicoes_ui_actions_t repeat_vertical_actions(
    retropicoes_menu_repeat_t *repeat,
    uint32_t held_compat,
    uint32_t pressed_compat)
{
    const auto held = retropicoes_ti83_menu_actions(
        held_compat, g_menu_profile);
    const auto pressed = retropicoes_ti83_menu_actions(
        pressed_compat, g_menu_profile);
    const auto vertical = retropicoes_menu_repeat_update(
        repeat, held, pressed, compat_ticks_ms());
    return (pressed & ~(RETROPICOES_UI_UP | RETROPICOES_UI_DOWN)) |
           vertical;
}

void reset_menu_owner()
{
    retropicoes_menu_owner_reset(&g_menu_owner);
    g_menu_profile = RETROPICOES_CONTROLLER_UNKNOWN;
}

void update_menu_owner()
{
    retropicoes_input_stats_t stats{};
    uint32_t source_actions[4];
    retropicoes_input_get_stats(&stats);
    for (unsigned i = 0; i < 4u; ++i)
        source_actions[i] = retropicoes_ti83_menu_actions(
            stats.source_masks[i], RETROPICOES_CONTROLLER_UNKNOWN);
    if (g_menu_owner.active_controller < 2u &&
        stats.usb_name[g_menu_owner.active_controller] == nullptr)
        retropicoes_menu_owner_disconnected(
            &g_menu_owner, g_menu_owner.active_controller);
    const size_t owner = retropicoes_menu_owner_update(
        &g_menu_owner, source_actions, 4u);
    if (owner < 2u) {
        g_menu_profile = retropicoes_menu_controller_profile_from_name(
            stats.usb_name[owner]);
    } else if (owner < 4u) {
        g_menu_profile =
            stats.nes_pad_type[owner - 2u] == RETROPICOES_NESPAD_SNES
                ? RETROPICOES_CONTROLLER_SNES
                : RETROPICOES_CONTROLLER_NES;
    }
}

void settings_get_renderer(void *, retropicoes_menu_renderer_t *renderer)
{
    *renderer = retropicoes_menu_nintendo_renderer();
}
void settings_reset_owner(void *) { reset_menu_owner(); }
void settings_update_owner(void *) { update_menu_owner(); }
retropicoes_controller_profile_t settings_controller_profile(void *)
{
    return g_menu_profile;
}
retropicoes_ui_actions_t settings_read_actions(
    void *, retropicoes_menu_repeat_t *repeat)
{
    return repeat_vertical_actions(
        repeat, down_compat_mask(), pressed_compat_mask());
}
const retropicoes_settings_platform_t kSettingsPlatform{
    nullptr, settings_get_renderer, settings_reset_owner,
    settings_update_owner, settings_controller_profile, settings_read_actions
};

bool at_rom_root(const char *path)
{
    return std::strcmp(path, "/roms/TI83") == 0;
}

void parent_directory(char *path)
{
    if (at_rom_root(path)) return;
    char *slash = std::strrchr(path, '/');
    if (slash != nullptr && slash > path) *slash = '\0';
}

bool join_path(char *output, size_t capacity, const char *directory, const char *name)
{
    const size_t dir_length=std::strlen(directory), name_length=std::strlen(name);
    if (dir_length+name_length+2u>capacity) return false;
    std::memcpy(output,directory,dir_length);
    output[dir_length]='/';
    std::memcpy(output+dir_length+1u,name,name_length+1u);
    return true;
}

unsigned scan_roms(const char *path)
{
    compat_dir_t *directory = compat_dir_open(path);
    unsigned count = 0u;
    if (directory == nullptr) return 0u;
    const char *name;
    while (count < kMaxRoms &&
           (name = compat_dir_read(directory)) != nullptr) {
        if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0)
            continue;
        if (std::strlen(name) >= kNameCapacity ||
            std::strlen(path) + std::strlen(name) + 2u > 320u) continue;
        char full[320];
        if (!join_path(full,sizeof(full),path,name)) continue;
        const bool is_directory =
            compat_path_type(full) == COMPAT_PATH_DIRECTORY;
        if (!is_directory && !ti83_program_extension(name))
            continue;
        std::snprintf(g_roms[count].name, sizeof(g_roms[count].name),
                      "%s", name);
        std::snprintf(g_roms[count].display, sizeof(g_roms[count].display),
                      "%s%s", is_directory ? "> " : "", name);
        g_roms[count].is_directory = is_directory;
        ++count;
    }
    compat_dir_close(directory);
    std::qsort(g_roms,count,sizeof(g_roms[0]),[](const void *left,const void *right)->int {
        const auto *a=static_cast<const RomEntry *>(left);
        const auto *b=static_cast<const RomEntry *>(right);
        if (a->is_directory != b->is_directory) return a->is_directory ? -1 : 1;
        return std::strcmp(a->name,b->name);
    });
    return count;
}

struct Ti83RomSource {
    const char *path;
    unsigned count;
};

size_t rom_source_count(void *context)
{
    return static_cast<Ti83RomSource *>(context)->count;
}

int rom_source_item(void *context, size_t index,
                    retropicoes_rom_item_view_t *item)
{
    auto *source = static_cast<Ti83RomSource *>(context);
    if (item == nullptr || index >= source->count) return 0;
    item->label = g_roms[index].display;
    item->kind = g_roms[index].is_directory
                     ? RETROPICOES_ROM_ITEM_DIRECTORY
                     : RETROPICOES_ROM_ITEM_ROM;
    return 1;
}

const char *rom_source_path(void *context)
{
    return static_cast<Ti83RomSource *>(context)->path;
}

retropicoes_rom_source_t make_rom_source(Ti83RomSource *context)
{
    retropicoes_rom_source_t source{};
    source.version = RETROPICOES_ROM_SOURCE_VERSION;
    source.struct_size = sizeof(source);
    source.context = context;
    source.title = "-- TI-83/84 GAMES --";
    source.empty_message = "ADD CLASSIC .8XP GAMES";
    source.demo_label = "DEMO / TEST GAME (SIMULATED)";
    source.count = rom_source_count;
    source.item = rom_source_item;
    source.path = rom_source_path;
    return source;
}

void draw_rom_browser(const retropicoes_rom_browser_t &browser)
{
    compat_framebuffer_t *fb = compat_framebuffer();
    if (fb == nullptr) return;
    auto renderer = rom_browser_renderer();
    auto surface = menu_surface(fb);
    if (!retropicoes_rom_browser_render(
            &browser, &renderer, &surface,
            retropicoes_menu_controller_labels(g_menu_profile))) return;
    compat_present();
}

void draw_message(const char *title, const char *detail)
{
    compat_framebuffer_t *fb = compat_framebuffer();
    if (fb == nullptr) return;
    for (unsigned y=0; y<fb->height; ++y)
        for (unsigned x=0; x<fb->width; ++x) fb->pixels[y*fb->stride+x]=0;
    auto renderer=retropicoes_menu_nintendo_renderer();
    auto surface=menu_surface(fb);
    renderer.draw_text(&surface,16,78,title,0xfd68,1,36,renderer.text_context);
    renderer.draw_text(&surface,16,120,detail,0xffff,1,48,renderer.text_context);
    renderer.draw_text(&surface,16,166,"PRESS A OR B FOR ROM MENU",0x5d7f,1,48,renderer.text_context);
    compat_present();
}

void wait_for_menu_button()
{
    for (;;) {
        compat_poll_input();
        if (compat_key_pressed(COMPAT_KEY_JUMP) ||
            compat_key_pressed(COMPAT_KEY_ACTION)) return;
        compat_delay_ms(16u);
    }
}

struct Ti83Session {
    char path[320];
    char error[96];
    bool demo;
};

retropicoes_menu_result_t lifecycle_start(
    void *opaque, size_t, retropicoes_rom_item_kind_t kind)
{
    auto *session = static_cast<Ti83Session *>(opaque);
    session->demo = kind == RETROPICOES_ROM_ITEM_DEMO;
    if (kind != RETROPICOES_ROM_ITEM_ROM && kind != RETROPICOES_ROM_ITEM_DEMO)
        return RETROPICOES_MENU_RESULT_INVALID;
    return retropicoes_ti83_frontend_start(
        session->demo ? nullptr : session->path, session->error,
        sizeof(session->error)) == 0 ? RETROPICOES_MENU_RESULT_OK :
                                RETROPICOES_MENU_RESULT_FAILED;
}

retropicoes_menu_result_t lifecycle_pause(void *)
{
    return retropicoes_ti83_frontend_pause() == 0 ?
        RETROPICOES_MENU_RESULT_OK : RETROPICOES_MENU_RESULT_FAILED;
}

retropicoes_menu_result_t lifecycle_resume(void *)
{
    return retropicoes_ti83_frontend_resume() == 0 ?
        RETROPICOES_MENU_RESULT_OK : RETROPICOES_MENU_RESULT_FAILED;
}

retropicoes_menu_result_t lifecycle_reset(void *opaque)
{
    auto *session = static_cast<Ti83Session *>(opaque);
    retropicoes_ti83_frontend_stop();
    return retropicoes_ti83_frontend_start(
        session->demo ? nullptr : session->path, session->error,
        sizeof(session->error)) == 0 ? RETROPICOES_MENU_RESULT_OK :
                                RETROPICOES_MENU_RESULT_FAILED;
}

retropicoes_menu_result_t lifecycle_stop(void *)
{
    retropicoes_ti83_frontend_stop();
    return RETROPICOES_MENU_RESULT_OK;
}

retropicoes_menu_result_t lifecycle_return_loader(void *)
{
    retropicoes_return_to_bootloader();
    return RETROPICOES_MENU_RESULT_FAILED;
}

retropicoes_frontend_lifecycle_t lifecycle_callbacks(Ti83Session *session)
{
    retropicoes_frontend_lifecycle_t callbacks{};
    callbacks.version = RETROPICOES_FRONTEND_LIFECYCLE_VERSION;
    callbacks.struct_size = sizeof(callbacks);
    callbacks.context = session;
    callbacks.start = lifecycle_start;
    callbacks.pause = lifecycle_pause;
    callbacks.resume = lifecycle_resume;
    callbacks.reset = lifecycle_reset;
    callbacks.stop = lifecycle_stop;
    callbacks.return_loader = lifecycle_return_loader;
    return callbacks;
}

retropicoes_frontend_settings_action_t lifecycle_action(
    retropicoes_ti83_menu_request_t request)
{
    switch (request) {
    case RETROPICOES_TI83_MENU_REQUEST_RESET:
        return RETROPICOES_FRONTEND_SETTINGS_RESET;
    case RETROPICOES_TI83_MENU_REQUEST_ROM_LIST:
        return RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER;
    case RETROPICOES_TI83_MENU_REQUEST_RETURN_LOADER:
        return RETROPICOES_FRONTEND_SETTINGS_RETURN_LOADER;
    default:
        return RETROPICOES_FRONTEND_SETTINGS_RESUME;
    }
}

}  // namespace

int main()
{
    if (compat_init() != 0) {
        std::printf("TI83 FATAL: HW16 platform initialization failed\n");
        return 1;
    }
    std::printf("TI83 READY: version=ti83-perf21-hotmain games=/roms/TI83 save=/SAVES/TI83 "
                "target=60Hz LCD=96x64 Z80 TI-83/83+/84+\n");
    retropicoes_ti83_settings_ui_init();

    Ti83Session session{};
    retropicoes_frontend_shell_t shell{};
    const auto callbacks = lifecycle_callbacks(&session);
    if (!retropicoes_frontend_shell_init(&shell, &callbacks)) {
        std::printf("TI83 FATAL: common lifecycle invalid\n");
        return 1;
    }

    char rom_directory[320] = "/roms/TI83";
    unsigned remembered_selection = 0u;
    unsigned parent_selections[32]{};
    unsigned directory_depth = 0u;
    for (;;) {
        const unsigned count = scan_roms(rom_directory);
        bool rescan = false;
        reset_menu_owner();
        Ti83RomSource source_context{rom_directory, count};
        const auto source = make_rom_source(&source_context);
        retropicoes_rom_browser_t browser;
        retropicoes_menu_repeat_t repeat;
        if (!retropicoes_rom_browser_open(&browser, &source, 18u)) {
            draw_message("TI-83/84 MENU ERROR", "ROM BROWSER CONTRACT INVALID");
            wait_for_menu_button();
            continue;
        }
        if (count != 0u) {
            if (remembered_selection >= count) remembered_selection = count - 1u;
            browser.list.selected = remembered_selection;
            if (browser.list.selected >= browser.list.page_rows)
                browser.list.first_visible =
                    browser.list.selected - browser.list.page_rows + 1u;
        }
        retropicoes_menu_repeat_reset(&repeat);

        for (;;) {
            draw_rom_browser(browser);
            compat_poll_input();
            update_menu_owner();
            const uint32_t pressed = pressed_compat_mask();
            const auto actions = repeat_vertical_actions(
                &repeat, down_compat_mask(), pressed);
            const auto event = retropicoes_rom_browser_update(
                &browser, actions);
            if (event.type == RETROPICOES_ROM_BROWSER_EVENT_SETTINGS) {
                if (retropicoes_frontend_shell_open_settings(&shell) ==
                    RETROPICOES_MENU_RESULT_OK) {
                    const auto request = retropicoes_ti83_settings_ui_run(
                        0, &kSettingsPlatform);
                    (void)retropicoes_frontend_shell_settings_action(
                        &shell, lifecycle_action(request));
                }
                retropicoes_menu_repeat_reset(&repeat);
                reset_menu_owner();
            }
            if (event.type == RETROPICOES_ROM_BROWSER_EVENT_BACK) {
                if (at_rom_root(rom_directory)) {
                    if (retropicoes_frontend_shell_open_settings(&shell) == RETROPICOES_MENU_RESULT_OK) {
                        const auto request = retropicoes_ti83_settings_ui_run(0, &kSettingsPlatform);
                        (void)retropicoes_frontend_shell_settings_action(&shell, lifecycle_action(request));
                    }
                    continue;
                }
                parent_directory(rom_directory);
                remembered_selection = directory_depth ? parent_selections[--directory_depth] : 0u;
                rescan = true;
                break;
            }
            if (event.type == RETROPICOES_ROM_BROWSER_EVENT_OPEN) {
                if (event.item_kind == RETROPICOES_ROM_ITEM_DEMO) {
                    session.demo = true;
                    break;
                }
                session.demo = false;
                const auto selected_rom = static_cast<unsigned>(event.source_index);
                if (event.item_kind == RETROPICOES_ROM_ITEM_DIRECTORY) {
                    if (directory_depth >= 32u) continue;
                    parent_selections[directory_depth++] = selected_rom;
                    const size_t used = std::strlen(rom_directory);
                    std::snprintf(rom_directory + used,
                                  sizeof(rom_directory) - used, "/%s",
                                  g_roms[selected_rom].name);
                    remembered_selection = 0u;
                    rescan = true;
                } else remembered_selection = selected_rom;
                break;
            }
            compat_delay_ms(16u);
        }
        if (rescan) continue;

        if (!session.demo) {
            if (!join_path(session.path,sizeof(session.path),rom_directory,
                           g_roms[remembered_selection].name)) continue;
        }
        draw_message("TI-83/84 LOADING", session.demo ? "SIMULATED TEST GAME" : g_roms[remembered_selection].name);
        std::printf("TI83 ROM: %s\n", session.demo ? "SIMULATED TEST GAME" : session.path);
        if (retropicoes_frontend_shell_launch(
                &shell, remembered_selection, session.demo ? RETROPICOES_ROM_ITEM_DEMO : RETROPICOES_ROM_ITEM_ROM) !=
            RETROPICOES_MENU_RESULT_OK) {
            draw_message("TI-83/84 LOAD ERROR", session.error);
            std::printf("TI83 LOAD ERROR: %s\n", session.error);
            wait_for_menu_button();
            continue;
        }

        static RetroPerfReport perf;
        perf.begin("TI83", compat_ticks_us());
        uint64_t perf_window = compat_ticks_us(), heartbeat = perf_window;
        unsigned frame_fraction = 0;
        uint64_t next_frame_deadline = compat_ticks_us() + kFrameBudgetUs;
        for (;;) {
            const int result = retropicoes_ti83_frontend_run_frame();
            retropicoes_ti83_frontend_stats_t stats;
            retropicoes_ti83_frontend_get_stats(&stats);
            if (result == 2) {
                if (retropicoes_frontend_shell_open_settings(&shell) !=
                    RETROPICOES_MENU_RESULT_OK) {
                    std::printf("TI83 MENU ERROR: pause failed\n");
                    continue;
                }
                const auto request = retropicoes_ti83_settings_ui_run(
                    1, &kSettingsPlatform);
                const auto action_result =
                    retropicoes_frontend_shell_settings_action(
                        &shell, lifecycle_action(request));
                if (action_result != RETROPICOES_MENU_RESULT_OK)
                    std::printf("TI83 MENU ERROR: action=%u result=%u\n",
                                static_cast<unsigned>(request),
                                static_cast<unsigned>(action_result));
                if (shell.screen == RETROPICOES_FRONTEND_SETTINGS)
                    (void)retropicoes_frontend_shell_settings_action(
                        &shell, RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER);
                if (shell.screen == RETROPICOES_FRONTEND_RUNNING) {
                    perf.begin("TI83", compat_ticks_us());
                    perf_window = compat_ticks_us(); heartbeat = perf_window;
                    next_frame_deadline = compat_ticks_us() + kFrameBudgetUs;
                    continue;
                }
                break;
            }
            if (result != 0) {
                if (result < 0) {
                    std::snprintf(session.error, sizeof(session.error),
                                  "CORE FAULT %d", result);
                    draw_message("TI-83/84 RUNTIME ERROR", session.error);
                    std::printf("TI83 RUNTIME ERROR: %d\n", result);
                    wait_for_menu_button();
                }
                (void)retropicoes_frontend_shell_game_finished(&shell);
                break;
            }

            wait_for_frame_deadline(next_frame_deadline);
            const uint64_t report_time = compat_ticks_us();
            perf.frame(stats, report_time - perf_window >= 1000000u ? report_time : perf_window);
            if (report_time - perf_window >= 1000000u) perf_window = report_time;
            if (report_time - heartbeat >= 10000000u && perf.idle()) {
                perf.queue_diagnostic("HEARTBEAT TI83\r\n"); heartbeat = report_time;
            }
            perf.pump();

            frame_fraction += 40u;
            next_frame_deadline += 16666u + frame_fraction / 60u;
            frame_fraction %= 60u;
            if (report_time > next_frame_deadline + 2u * kFrameBudgetUs)
                next_frame_deadline = report_time + kFrameBudgetUs;
        }
    }
}
