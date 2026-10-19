/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "perf_report.h"
#include <cstdio>
#include <cstring>

extern "C" {
#include "compat.h"
#include "frontend.h"
#include "frontend_shell.h"
#include "input_backend.h"
#include "input_mapping.h"
#include "menu_adapter.h"
#include "menu_input.h"
#include "menu_render.h"
#include "menu_runtime.h"
#include "menu_state.h"
#include "path_adapter.h"
#include "rom_browser.h"
#include "runtime.h"
#include "settings_menu.h"
#include "settings_ui.h"
}

namespace {
constexpr unsigned kMaxRoms = 128;
constexpr unsigned kNameCapacity = 96;
constexpr uint32_t kFrameBudgetUs = 16673u;
constexpr uint32_t kDelayGuardUs = 500u;

struct RomEntry {
    char name[kNameCapacity];
    char display[kNameCapacity + 2u];
    int default_profile;
    bool is_directory;
};

RomEntry g_roms[kMaxRoms];
retropicoes_menu_owner_t g_menu_owner{RETROPICOES_MENU_NO_CONTROLLER};
retropicoes_controller_profile_t g_menu_profile = RETROPICOES_CONTROLLER_UNKNOWN;

void wait_for_frame_deadline(uint64_t deadline_us) {
    for (;;) {
        const uint64_t now_us = compat_ticks_us();
        if (now_us >= deadline_us)
            return;

        const uint64_t remaining_us = deadline_us - now_us;
        if (remaining_us > 1000u + kDelayGuardUs) {
            const uint32_t delay_ms = static_cast<uint32_t>((remaining_us - kDelayGuardUs) / 1000u);
            if (delay_ms != 0u)
                compat_delay_ms(delay_ms);
        }
    }
}

static const uint8_t kFont[36][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
    {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},     {14, 17, 19, 21, 25, 17, 14},
    {4, 12, 4, 4, 4, 4, 14},      {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},   {14, 16, 16, 30, 17, 17, 14},
    {31, 1, 2, 4, 8, 8, 8},       {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14}};

void pixel(compat_framebuffer_t *fb, int x, int y, uint16_t color) {
    if (fb == nullptr || x < 0 || y < 0 || x >= static_cast<int>(fb->width) ||
        y >= static_cast<int>(fb->height))
        return;
    fb->pixels[static_cast<size_t>(y) * fb->stride + x] = color;
}

void fill(compat_framebuffer_t *fb, int x, int y, int w, int h, uint16_t color) {
    for (int py = y; py < y + h; ++py)
        for (int px = x; px < x + w; ++px)
            pixel(fb, px, py, color);
}

void character(compat_framebuffer_t *fb, int x, int y, char ch, uint16_t color, int scale) {
    const uint8_t *glyph = nullptr;
    if (ch >= 'a' && ch <= 'z')
        ch = static_cast<char>(ch - 'a' + 'A');
    if (ch >= 'A' && ch <= 'Z')
        glyph = kFont[ch - 'A'];
    else if (ch >= '0' && ch <= '9')
        glyph = kFont[26 + ch - '0'];
    if (glyph != nullptr) {
        for (int row = 0; row < 7; ++row)
            for (int column = 0; column < 5; ++column)
                if (glyph[row] & (1u << (4 - column)))
                    fill(fb, x + column * scale, y + row * scale, scale, scale, color);
    } else if (ch == '.') {
        fill(fb, x + 2 * scale, y + 6 * scale, scale, scale, color);
    } else if (ch == '-') {
        fill(fb, x, y + 3 * scale, 5 * scale, scale, color);
    } else if (ch == ':') {
        fill(fb, x + 2 * scale, y + 2 * scale, scale, scale, color);
        fill(fb, x + 2 * scale, y + 5 * scale, scale, scale, color);
    } else if (ch == '/') {
        for (int row = 0; row < 7; ++row)
            fill(fb, x + (4 - row * 4 / 6) * scale, y + row * scale, scale, scale, color);
    } else if (ch == '>') {
        for (int row = 0; row < 7; ++row) {
            const int column = row <= 3 ? row : 6 - row;
            fill(fb, x + column * scale, y + row * scale, scale, scale, color);
        }
    }
}

void text(compat_framebuffer_t *fb, int x, int y, const char *value, uint16_t color, int scale,
          unsigned max_chars = 52) {
    unsigned count = 0;
    while (value != nullptr && *value != '\0' && count++ < max_chars) {
        character(fb, x, y, *value++, color, scale);
        x += 6 * scale;
    }
}

retropicoes_menu_renderer_t rom_browser_renderer() {
    retropicoes_menu_renderer_t renderer = retropicoes_menu_nintendo_renderer();
    const auto *settings = retropicoes_studio2_settings_ui_current();
    const uint16_t fg = retropicoes_studio2_theme_color(settings->foreground_color);
    const uint16_t bg = retropicoes_studio2_theme_color(settings->background_color);
    renderer.theme = {bg, fg, fg, fg, fg, bg, 0xf800u};
    return renderer;
}

retropicoes_menu_surface_t menu_surface(compat_framebuffer_t *fb) {
    return {fb->pixels, fb->stride, fb->width, fb->height};
}

uint32_t pressed_compat_mask() {
    uint32_t result = 0u;
    for (unsigned key = 0; key < COMPAT_KEY_COUNT; ++key)
        if (compat_key_pressed(static_cast<compat_key_t>(key)))
            result |= 1u << key;
    return result;
}

uint32_t down_compat_mask() {
    uint32_t result = 0u;
    for (unsigned key = 0; key < COMPAT_KEY_COUNT; ++key)
        if (compat_key_down(static_cast<compat_key_t>(key)))
            result |= 1u << key;
    return result;
}

retropicoes_ui_actions_t repeat_vertical_actions(retropicoes_menu_repeat_t *repeat,
                                                 uint32_t held_compat, uint32_t pressed_compat) {
    const auto held = retropicoes_studio2_menu_actions(held_compat, g_menu_profile);
    const auto pressed = retropicoes_studio2_menu_actions(pressed_compat, g_menu_profile);
    const auto vertical = retropicoes_menu_repeat_update(repeat, held, pressed, compat_ticks_ms());
    return (pressed & ~(RETROPICOES_UI_UP | RETROPICOES_UI_DOWN)) | vertical;
}

void reset_menu_owner() {
    retropicoes_menu_owner_reset(&g_menu_owner);
    g_menu_profile = RETROPICOES_CONTROLLER_UNKNOWN;
}

void update_menu_owner() {
    retropicoes_input_stats_t stats{};
    uint32_t source_actions[4];
    retropicoes_input_get_stats(&stats);
    for (unsigned i = 0; i < 4u; ++i)
        source_actions[i] =
            retropicoes_studio2_menu_actions(stats.source_masks[i], RETROPICOES_CONTROLLER_UNKNOWN);
    if (g_menu_owner.active_controller < 2u &&
        stats.usb_name[g_menu_owner.active_controller] == nullptr)
        retropicoes_menu_owner_disconnected(&g_menu_owner, g_menu_owner.active_controller);
    const size_t owner = retropicoes_menu_owner_update(&g_menu_owner, source_actions, 4u);
    if (owner < 2u) {
        g_menu_profile = retropicoes_menu_controller_profile_from_name(stats.usb_name[owner]);
    } else if (owner < 4u) {
        g_menu_profile = stats.nes_pad_type[owner - 2u] == RETROPICOES_NESPAD_SNES
                             ? RETROPICOES_CONTROLLER_SNES
                             : RETROPICOES_CONTROLLER_NES;
    }
}

void settings_get_renderer(void *, retropicoes_menu_renderer_t *renderer) {
    *renderer = retropicoes_menu_nintendo_renderer();
}
void settings_reset_owner(void *) { reset_menu_owner(); }
void settings_update_owner(void *) { update_menu_owner(); }
retropicoes_controller_profile_t settings_controller_profile(void *) { return g_menu_profile; }
retropicoes_ui_actions_t settings_read_actions(void *, retropicoes_menu_repeat_t *repeat) {
    return repeat_vertical_actions(repeat, down_compat_mask(), pressed_compat_mask());
}
const retropicoes_settings_platform_t kSettingsPlatform{nullptr,
                                                        settings_get_renderer,
                                                        settings_reset_owner,
                                                        settings_update_owner,
                                                        settings_controller_profile,
                                                        settings_read_actions};

bool at_rom_root(const char *path) { return std::strcmp(path, "/roms/STUDIO2") == 0; }

void parent_directory(char *path) {
    if (at_rom_root(path))
        return;
    char *slash = std::strrchr(path, '/');
    if (slash != nullptr && slash > path)
        *slash = '\0';
}

const char *profile_name(int) { return "NTSC"; }

unsigned scan_roms(const char *path) {
    compat_dir_t *directory = compat_dir_open(path);
    unsigned count = 0u;
    if (directory == nullptr)
        return 0u;
    const char *name;
    while (count < kMaxRoms && (name = compat_dir_read(directory)) != nullptr) {
        if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0)
            continue;
        if (std::strlen(name) >= kNameCapacity)
            continue;
        char full[320];
        if (std::snprintf(full, sizeof(full), "%s/%s", path, name) >=
            static_cast<int>(sizeof(full)))
            continue;
        const bool is_directory = compat_path_type(full) == COMPAT_PATH_DIRECTORY;
        int profile = 0;
        if (!is_directory && !retropicoes_studio2_rom_profile(name, &profile))
            continue;
        std::snprintf(g_roms[count].name, sizeof(g_roms[count].name), "%s", name);
        std::snprintf(g_roms[count].display, sizeof(g_roms[count].display), "%s%s",
                      is_directory ? "> " : "", name);
        g_roms[count].default_profile = profile;
        g_roms[count].is_directory = is_directory;
        ++count;
    }
    compat_dir_close(directory);
    return count;
}

struct Studio2RomSource {
    const char *path;
    unsigned count;
};

size_t rom_source_count(void *context) { return static_cast<Studio2RomSource *>(context)->count; }

int rom_source_item(void *context, size_t index, retropicoes_rom_item_view_t *item) {
    auto *source = static_cast<Studio2RomSource *>(context);
    if (item == nullptr || index >= source->count)
        return 0;
    item->label = g_roms[index].display;
    item->kind =
        g_roms[index].is_directory ? RETROPICOES_ROM_ITEM_DIRECTORY : RETROPICOES_ROM_ITEM_ROM;
    return 1;
}

const char *rom_source_path(void *context) {
    return static_cast<Studio2RomSource *>(context)->path;
}

retropicoes_rom_source_t make_rom_source(Studio2RomSource *context) {
    retropicoes_rom_source_t source{};
    source.version = RETROPICOES_ROM_SOURCE_VERSION;
    source.struct_size = sizeof(source);
    source.context = context;
    source.title = "-- RCA Studio II --";
    source.empty_message = "NO COMPATIBLE RCA Studio II ROMS";
    source.demo_label = "DEMO / TEST GAME (SIMULATED)";
    source.count = rom_source_count;
    source.item = rom_source_item;
    source.path = rom_source_path;
    return source;
}

void draw_rom_browser(const retropicoes_rom_browser_t &browser, int profile) {
    compat_framebuffer_t *fb = compat_framebuffer();
    if (fb == nullptr)
        return;
    auto renderer = rom_browser_renderer();
    auto surface = menu_surface(fb);
    if (!retropicoes_rom_browser_render(&browser, &renderer, &surface,
                                        retropicoes_menu_controller_labels(g_menu_profile)))
        return;
    (void)profile;
    renderer.draw_text(&surface, 7, 227, "START=1  BANK IN SETTINGS", renderer.theme.foreground, 1u,
                       34u, renderer.text_context);
    compat_present();
}

void draw_message(const char *title, const char *detail) {
    compat_framebuffer_t *fb = compat_framebuffer();
    if (fb == nullptr)
        return;
    fill(fb, 0, 0, fb->width, fb->height, 0x0000u);
    text(fb, 34, 78, title, 0xFD68u, 2, 22);
    text(fb, 16, 120, detail, 0xFFFFu, 1, 48);
    text(fb, 52, 166, "PRESS A OR B FOR ROM MENU", 0x5D7Fu, 1);
    compat_present();
}

void wait_for_menu_button() {
    for (;;) {
        compat_poll_input();
        if (compat_key_pressed(COMPAT_KEY_JUMP) || compat_key_pressed(COMPAT_KEY_ACTION))
            return;
        compat_delay_ms(16u);
    }
}

struct Studio2Session {
    char path[320];
    char error[96];
    int profile;
};

retropicoes_menu_result_t lifecycle_start(void *opaque, size_t, retropicoes_rom_item_kind_t kind) {
    auto *session = static_cast<Studio2Session *>(opaque);
    if (kind != RETROPICOES_ROM_ITEM_ROM)
        return RETROPICOES_MENU_RESULT_INVALID;
    return retropicoes_studio2_frontend_start(session->path, session->profile, session->error,
                                              sizeof(session->error)) == 0
               ? RETROPICOES_MENU_RESULT_OK
               : RETROPICOES_MENU_RESULT_FAILED;
}

retropicoes_menu_result_t lifecycle_pause(void *) {
    return retropicoes_studio2_frontend_pause() == 0 ? RETROPICOES_MENU_RESULT_OK
                                                     : RETROPICOES_MENU_RESULT_FAILED;
}

retropicoes_menu_result_t lifecycle_resume(void *) {
    return retropicoes_studio2_frontend_resume() == 0 ? RETROPICOES_MENU_RESULT_OK
                                                      : RETROPICOES_MENU_RESULT_FAILED;
}

retropicoes_menu_result_t lifecycle_reset(void *opaque) {
    auto *session = static_cast<Studio2Session *>(opaque);
    retropicoes_studio2_frontend_stop();
    return retropicoes_studio2_frontend_start(session->path, session->profile, session->error,
                                              sizeof(session->error)) == 0
               ? RETROPICOES_MENU_RESULT_OK
               : RETROPICOES_MENU_RESULT_FAILED;
}

retropicoes_menu_result_t lifecycle_stop(void *) {
    retropicoes_studio2_frontend_stop();
    return RETROPICOES_MENU_RESULT_OK;
}

retropicoes_menu_result_t lifecycle_return_loader(void *) {
    retropicoes_return_to_bootloader();
    return RETROPICOES_MENU_RESULT_FAILED;
}

retropicoes_frontend_lifecycle_t lifecycle_callbacks(Studio2Session *session) {
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

retropicoes_frontend_settings_action_t
lifecycle_action(retropicoes_studio2_menu_request_t request) {
    switch (request) {
    case RETROPICOES_STUDIO2_MENU_REQUEST_RESET:
        return RETROPICOES_FRONTEND_SETTINGS_RESET;
    case RETROPICOES_STUDIO2_MENU_REQUEST_ROM_LIST:
        return RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER;
    case RETROPICOES_STUDIO2_MENU_REQUEST_RETURN_LOADER:
        return RETROPICOES_FRONTEND_SETTINGS_RETURN_LOADER;
    default:
        return RETROPICOES_FRONTEND_SETTINGS_RESUME;
    }
}

} // namespace

int main() {
    if (compat_init() != 0) {
        std::printf("STUDIO2 FATAL: HW16 platform initialization failed\n");
        return 1;
    }
    std::printf("STUDIO2 READY: rom=/roms/STUDIO2 save=/SAVES/STUDIO2 "
                "CPU=1.760MHz video=59.978Hz\n");
    retropicoes_studio2_settings_ui_init();

    Studio2Session session{};
    retropicoes_frontend_shell_t shell{};
    const auto callbacks = lifecycle_callbacks(&session);
    if (!retropicoes_frontend_shell_init(&shell, &callbacks)) {
        std::printf("STUDIO2 FATAL: common lifecycle invalid\n");
        return 1;
    }

    char rom_directory[320] = "/roms/STUDIO2";
    unsigned remembered_selection = 0u;
    unsigned parents[32]{}, depth = 0;
    for (;;) {
        const unsigned count = scan_roms(rom_directory);
        bool rescan = false;
        reset_menu_owner();
        Studio2RomSource source_context{rom_directory, count};
        const auto source = make_rom_source(&source_context);
        retropicoes_rom_browser_t browser;
        retropicoes_menu_repeat_t repeat;
        if (!retropicoes_rom_browser_open(&browser, &source, 18u)) {
            draw_message("RCA Studio II MENU ERROR", "ROM BROWSER CONTRACT INVALID");
            wait_for_menu_button();
            continue;
        }
        if (count != 0u) {
            if (remembered_selection >= count)
                remembered_selection = count - 1u;
            browser.list.selected = remembered_selection;
            if (browser.list.selected >= browser.list.page_rows)
                browser.list.first_visible = browser.list.selected - browser.list.page_rows + 1u;
        }
        retropicoes_menu_repeat_reset(&repeat);
        int profile = count != 0u ? g_roms[browser.list.selected].default_profile : 0;

        for (;;) {
            draw_rom_browser(browser, profile);
            compat_poll_input();
            update_menu_owner();
            const uint32_t pressed = pressed_compat_mask();
            const auto actions = repeat_vertical_actions(&repeat, down_compat_mask(), pressed);
            const auto event = retropicoes_rom_browser_update(&browser, actions);
            if (event.type == RETROPICOES_ROM_BROWSER_EVENT_REDRAW && count != 0u &&
                browser.list.selected < count)
                profile = g_roms[browser.list.selected].default_profile;
            if (event.type == RETROPICOES_ROM_BROWSER_EVENT_SETTINGS) {
                if (retropicoes_frontend_shell_open_settings(&shell) ==
                    RETROPICOES_MENU_RESULT_OK) {
                    const auto request = retropicoes_studio2_settings_ui_run(0, &kSettingsPlatform);
                    (void)retropicoes_frontend_shell_settings_action(&shell,
                                                                     lifecycle_action(request));
                }
                retropicoes_menu_repeat_reset(&repeat);
                reset_menu_owner();
            }
            if (event.type == RETROPICOES_ROM_BROWSER_EVENT_BACK) {
                if (at_rom_root(rom_directory))
                    continue;
                parent_directory(rom_directory);
                remembered_selection = depth ? parents[--depth] : 0u;
                rescan = true;
                break;
            }
            if (event.type == RETROPICOES_ROM_BROWSER_EVENT_OPEN) {
                if (event.item_kind == RETROPICOES_ROM_ITEM_DEMO) {
                    draw_message("RCA Studio II DEMO", "NO ROM INSTALLED");
                    wait_for_menu_button();
                    retropicoes_menu_repeat_reset(&repeat);
                    reset_menu_owner();
                    continue;
                }
                const auto selected_rom = static_cast<unsigned>(event.source_index);
                if (event.item_kind == RETROPICOES_ROM_ITEM_DIRECTORY) {
                    const size_t used = std::strlen(rom_directory);
                    if (depth >= 32u ||
                        used + 1u + std::strlen(g_roms[selected_rom].name) >= sizeof(rom_directory))
                        continue;
                    parents[depth++] = selected_rom;
                    std::snprintf(rom_directory + used, sizeof(rom_directory) - used, "/%s",
                                  g_roms[selected_rom].name);
                    remembered_selection = 0u;
                    rescan = true;
                } else
                    remembered_selection = selected_rom;
                break;
            }
            compat_delay_ms(16u);
        }
        if (rescan)
            continue;

        std::snprintf(session.path, sizeof(session.path), "%s/%s", rom_directory,
                      g_roms[remembered_selection].name);
        session.profile = profile;
        draw_message("RCA Studio II LOADING", g_roms[remembered_selection].name);
        std::printf("STUDIO2 ROM: path=%s profile=%s\n", session.path, profile_name(profile));
        if (retropicoes_frontend_shell_launch(&shell, remembered_selection,
                                              RETROPICOES_ROM_ITEM_ROM) !=
            RETROPICOES_MENU_RESULT_OK) {
            draw_message("RCA Studio II LOAD ERROR", session.error);
            std::printf("STUDIO2 LOAD ERROR: %s\n", session.error);
            wait_for_menu_button();
            continue;
        }

        static RetroPerfReport perf;
        perf.begin("STUDIO2", compat_ticks_us(), kFrameBudgetUs, 600u);
        uint32_t frame_fraction = 0;
        uint64_t next_frame_deadline = compat_ticks_us() + 16672u;
        uint64_t heartbeat_at = compat_ticks_us() + 10000000u;
        uint64_t input_debug_at = compat_ticks_us() + 1000000u;
        for (;;) {
            const int result = retropicoes_studio2_frontend_run_frame();
            retropicoes_studio2_frontend_stats_t stats;
            retropicoes_studio2_frontend_get_stats(&stats);
            if (result == 2) {
                if (retropicoes_frontend_shell_open_settings(&shell) !=
                    RETROPICOES_MENU_RESULT_OK) {
                    std::printf("STUDIO2 MENU ERROR: pause failed\n");
                    continue;
                }
                while (!perf.idle()) {
                    perf.pump();
                    compat_delay_ms(1u);
                }
                const auto request = retropicoes_studio2_settings_ui_run(1, &kSettingsPlatform);
                const auto action_result =
                    retropicoes_frontend_shell_settings_action(&shell, lifecycle_action(request));
                if (action_result != RETROPICOES_MENU_RESULT_OK)
                    std::printf("STUDIO2 MENU ERROR: action=%u result=%u\n",
                                static_cast<unsigned>(request),
                                static_cast<unsigned>(action_result));
                if (shell.screen == RETROPICOES_FRONTEND_SETTINGS)
                    (void)retropicoes_frontend_shell_settings_action(
                        &shell, RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER);
                if (shell.screen == RETROPICOES_FRONTEND_RUNNING) {
                    perf.begin("STUDIO2", compat_ticks_us(), kFrameBudgetUs, 600u);
                    next_frame_deadline = compat_ticks_us() + kFrameBudgetUs;
                    heartbeat_at = compat_ticks_us() + 10000000u;
                    input_debug_at = compat_ticks_us() + 1000000u;
                    frame_fraction = 0;
                    continue;
                }
                break;
            }
            if (result != 0) {
                if (result < 0) {
                    std::snprintf(session.error, sizeof(session.error), "CORE FAULT %d", result);
                    draw_message("RCA Studio II RUNTIME ERROR", session.error);
                    std::printf("STUDIO2 RUNTIME ERROR: %d\n", result);
                    wait_for_menu_button();
                }
                (void)retropicoes_frontend_shell_game_finished(&shell);
                break;
            }

            wait_for_frame_deadline(next_frame_deadline);
            const uint64_t report_time = compat_ticks_us();
            perf.frame(stats, report_time);
            perf.pump();
            if (report_time >= input_debug_at && perf.idle()) {
                retropicoes_studio2_input_debug_t input{};
                retropicoes_studio2_frontend_get_input_debug(&input);
                char record[384];
                std::snprintf(record, sizeof(record),
                              "INPUT STUDIO2 v=4 usb=%x raw=%02lx src=%02lx,%02lx,%02lx,%02lx "
                              "player=%c bank=%u A=%03x B=%03x seen=%03x,%03x raw_seen=%02lx "
                              "events=%lu ef_cycles=%lu,%lu latch=%u pc=%04x frame=%lu video=%08lx setup=%u\r\n",
                              static_cast<unsigned>(input.usb_connected),
                              static_cast<unsigned long>(input.raw_mask),
                              static_cast<unsigned long>(input.source_masks[0]),
                              static_cast<unsigned long>(input.source_masks[1]),
                              static_cast<unsigned long>(input.source_masks[2]),
                              static_cast<unsigned long>(input.source_masks[3]),
                              input.player ? 'B' : 'A', static_cast<unsigned>(input.bank),
                              static_cast<unsigned>(input.keys[0]),
                              static_cast<unsigned>(input.keys[1]),
                              static_cast<unsigned>(input.seen[0]),
                              static_cast<unsigned>(input.seen[1]),
                              static_cast<unsigned long>(input.raw_seen),
                              static_cast<unsigned long>(input.events),
                              static_cast<unsigned long>(input.ef_cycles[0]),
                              static_cast<unsigned long>(input.ef_cycles[1]),
                              static_cast<unsigned>(input.latch), static_cast<unsigned>(input.pc),
                              static_cast<unsigned long>(input.frames),
                              static_cast<unsigned long>(input.video_hash),
                              static_cast<unsigned>(input.setup_frames));
                if (perf.queue_diagnostic(record))
                    input_debug_at = report_time + 1000000u;
            }
            if (report_time >= heartbeat_at &&
                perf.queue_diagnostic("HEARTBEAT STUDIO2 running\r\n"))
                heartbeat_at = report_time + 10000000u;

            frame_fraction += 8u;
            next_frame_deadline += 16672u + frame_fraction / 11u;
            frame_fraction %= 11u;
            if (report_time > next_frame_deadline + 2u * kFrameBudgetUs)
                next_frame_deadline = report_time + kFrameBudgetUs;
        }
    }
}
