/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "settings_ui.h"
#include "settings_host.h"

#include <cstdio>
#include <cstring>

extern "C" {
#include "compat.h"
#include "frontend.h"
#include "input_backend.h"
}

namespace {
constexpr uint32_t kMagic = 0x53325453u;
constexpr uint16_t kVersion = 1u;
constexpr const char *kPath = "/settings_STUDIO2.dat";
retropicoes_studio2_settings_t g_settings{};
struct SettingsFile {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    retropicoes_studio2_settings_t settings;
};
using Item = retropicoes_settings_option_id_t;
constexpr Item QUIT = RETROPICOES_SETTINGS_OPTION_QUIT;
constexpr Item RESET = RETROPICOES_SETTINGS_OPTION_RESET;
constexpr Item LOADER = RETROPICOES_SETTINGS_OPTION_RETURN_LOADER;
constexpr Item SCANLINES = RETROPICOES_SETTINGS_OPTION_SCANLINES;
constexpr Item AUDIO = RETROPICOES_SETTINGS_OPTION_AUDIO;
constexpr Item FOREGROUND = RETROPICOES_SETTINGS_OPTION_MENU_FOREGROUND;
constexpr Item BACKGROUND = RETROPICOES_SETTINGS_OPTION_MENU_BACKGROUND;
constexpr Item CONTROLLER = RETROPICOES_SETTINGS_OPTION_CONTROLLER_TEST;
constexpr Item KEYPAD = static_cast<Item>(RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE);
constexpr Item PLAYER = static_cast<Item>(RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE + 1);
constexpr Item TENNIS = static_cast<Item>(RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE + 2);

unsigned build_items(bool in_game, Item *items) {
    uint32_t capabilities = RETROPICOES_SETTINGS_CAP_RETURN_LOADER |
                            RETROPICOES_SETTINGS_CAP_SCANLINES | RETROPICOES_SETTINGS_CAP_AUDIO |
                            RETROPICOES_SETTINGS_CAP_MENU_COLORS |
                            RETROPICOES_SETTINGS_CAP_CONTROLLER_TEST;
    const Item browser_custom[] = {KEYPAD, PLAYER};
    const Item game_custom[] = {KEYPAD, PLAYER, TENNIS};
    const Item *custom = browser_custom;
    size_t custom_count = sizeof(browser_custom) / sizeof(browser_custom[0]);
    if (in_game) {
        capabilities |= RETROPICOES_SETTINGS_CAP_QUIT | RETROPICOES_SETTINGS_CAP_RESET;
        custom = game_custom;
        custom_count = sizeof(game_custom) / sizeof(game_custom[0]);
        if (!retropicoes_studio2_frontend_can_start_tennis())
            --custom_count;
    }
    return static_cast<unsigned>(
        retropicoes_settings_build_option_order(items, 14u, capabilities, custom, custom_count));
}
const char *label(Item item) {
    const char *common = retropicoes_settings_common_label(item);
    if (common != nullptr)
        return common;
    switch (item) {
    case KEYPAD:
        return "Keypad Bank";
    case TENNIS:
        return "Start Tennis";
    default:
        return "Keypad Player";
    }
}
const char *value(Item item, const retropicoes_studio2_settings_t &s, char *buffer,
                  size_t capacity) {
    switch (item) {
    case AUDIO:
        return s.audio_enabled ? "ON" : "OFF";
    case SCANLINES:
        return s.scanlines_enabled ? "ON" : "OFF";
    case KEYPAD:
        std::snprintf(buffer, capacity, "%u", static_cast<unsigned>(s.keypad_bank));
        return buffer;
    case FOREGROUND:
        return retropicoes_studio2_theme_color_name(s.foreground_color);
    case BACKGROUND:
        return retropicoes_studio2_theme_color_name(s.background_color);
    case PLAYER:
        return s.keypad_player ? "B" : "A";
    default:
        return "";
    }
}
const char *help(Item item) {
    const char *common = retropicoes_settings_common_help(item);
    if (common != nullptr)
        return common;
    switch (item) {
    case KEYPAD:
        return "LEFT/RIGHT CHANGES DIGIT BANK; SAVE TO APPLY";
    case TENNIS:
        return "AUTO SETUP; WAIT 9S; UP/DOWN MOVES B PADDLE";
    default:
        return "CHOOSE LEFT A OR RIGHT B KEYPAD";
    }
}
void change(Item item, retropicoes_studio2_settings_t *s, int delta) {
    switch (item) {
    case AUDIO:
        s->audio_enabled ^= 1u;
        break;
    case SCANLINES:
        s->scanlines_enabled ^= 1u;
        break;
    case KEYPAD:
        s->keypad_bank = static_cast<uint8_t>((s->keypad_bank + 2 + delta) % 2);
        break;
    case FOREGROUND:
        s->foreground_color = static_cast<uint8_t>(
            (s->foreground_color + RETROPICOES_STUDIO2_THEME_COLOR_COUNT + delta) %
            RETROPICOES_STUDIO2_THEME_COLOR_COUNT);
        break;
    case BACKGROUND:
        s->background_color = static_cast<uint8_t>(
            (s->background_color + RETROPICOES_STUDIO2_THEME_COLOR_COUNT + delta) %
            RETROPICOES_STUDIO2_THEME_COLOR_COUNT);
        break;
    case PLAYER:
        s->keypad_player ^= 1u;
        break;
    default:
        break;
    }
    (void)retropicoes_studio2_settings_sanitize(s);
}
void apply(const retropicoes_studio2_settings_t &s) {
    retropicoes_studio2_frontend_config_t config{};
    config.audio_enabled = s.audio_enabled;
    config.scanlines_enabled = s.scanlines_enabled;
    config.keypad_bank = s.keypad_bank;
    config.keypad_player = s.keypad_player;
    retropicoes_studio2_frontend_set_config(&config);
}
void load() {
    SettingsFile stored{};
    retropicoes_studio2_settings_defaults(&g_settings);
    compat_file_t *file = compat_file_open(kPath, "rb");
    if (file != nullptr) {
        const bool valid = compat_file_read(&stored, 1u, sizeof(stored), file) == sizeof(stored) &&
                           stored.magic == kMagic && stored.version == kVersion &&
                           stored.size == sizeof(stored.settings);
        compat_file_close(file);
        if (valid)
            g_settings = stored.settings;
    }
    (void)retropicoes_studio2_settings_sanitize(&g_settings);
    apply(g_settings);
}
bool save(const retropicoes_studio2_settings_t &settings) {
    const SettingsFile stored{kMagic, kVersion, sizeof(settings), settings};
    compat_file_t *file = compat_file_open(kPath, "wb");
    if (file == nullptr)
        return false;
    const bool complete = compat_file_write(&stored, 1u, sizeof(stored), file) == sizeof(stored);
    const int closed = compat_file_close(file);
    return complete && closed == 0;
}
void fill_white(retropicoes_menu_surface_t *surface) {
    for (size_t y = 0; y < surface->height; ++y)
        for (size_t x = 0; x < surface->width; ++x)
            surface->pixels[y * surface->stride + x] = 0xffffu;
}
bool confirm(const retropicoes_settings_platform_t *platform, const char *title) {
    retropicoes_menu_repeat_t repeat{};
    bool released = false;
    for (;;) {
        compat_poll_input();
        platform->update_owner(platform->context);
        compat_framebuffer_t *fb = compat_framebuffer();
        if (fb == nullptr)
            return false;
        retropicoes_menu_surface_t surface{fb->pixels, fb->stride, fb->width, fb->height};
        fill_white(&surface);
        auto renderer = retropicoes_menu_nintendo_renderer();
        const auto *labels =
            retropicoes_menu_controller_labels(platform->controller_profile(platform->context));
        char hint[96];
        std::snprintf(hint, sizeof(hint), "%s: CONFIRM   %s: CANCEL", labels->confirm,
                      labels->cancel);
        renderer.draw_text(&surface, 24, 88, title, 0, 1, 36, renderer.text_context);
        renderer.draw_text(&surface, 24, 136, hint, 0, 1, 36, renderer.text_context);
        compat_present();
        const auto actions = platform->read_actions(platform->context, &repeat);
        if (!compat_key_down(COMPAT_KEY_ACTION) && !compat_key_down(COMPAT_KEY_JUMP) &&
            !compat_key_down(COMPAT_KEY_BACK))
            released = true;
        if (released && (actions & (RETROPICOES_UI_CANCEL | RETROPICOES_UI_MENU)))
            return false;
        if (released && (actions & RETROPICOES_UI_CONFIRM))
            return true;
        compat_delay_ms(16u);
    }
}
void controls(const retropicoes_settings_platform_t *platform) {
    retropicoes_menu_renderer_t renderer{};
    platform->get_renderer(platform->context, &renderer);
    do {
        compat_poll_input();
        compat_delay_ms(16u);
    } while (compat_key_down(COMPAT_KEY_ACTION) || compat_key_down(COMPAT_KEY_JUMP));
    retropicoes_menu_repeat_t repeat{};
    for (;;) {
        compat_poll_input();
        platform->update_owner(platform->context);
        retropicoes_input_stats_t stats{};
        retropicoes_input_get_stats(&stats);
        compat_framebuffer_t *fb = compat_framebuffer();
        if (fb == nullptr)
            return;
        retropicoes_menu_surface_t surface{fb->pixels, fb->stride, fb->width, fb->height};
        fill_white(&surface);
        char line[64];
        renderer.draw_text(&surface, 94, 8, "-- CONTROLLER TEST --", 0, 1, 26,
                           renderer.text_context);
        for (unsigned source = 0; source < 4; ++source) {
            std::snprintf(line, sizeof(line), "%s%u: %08X", source < 2 ? "USB" : "NES",
                          source % 2 + 1, static_cast<unsigned>(stats.source_masks[source]));
            renderer.draw_text(&surface, 10, 45 + static_cast<int>(source) * 24, line, 0, 1, 48,
                               renderer.text_context);
        }
        const auto *labels =
            retropicoes_menu_controller_labels(platform->controller_profile(platform->context));
        std::snprintf(line, sizeof(line), "PRESS %s TO GO BACK", labels->cancel);
        renderer.draw_text(&surface, 100, 225, line, 0, 1, 36, renderer.text_context);
        compat_present();
        if ((platform->read_actions(platform->context, &repeat) &
             (RETROPICOES_UI_CANCEL | RETROPICOES_UI_MENU)) != 0u)
            return;
        compat_delay_ms(16u);
    }
}
void render(const retropicoes_settings_platform_t *platform, const Item *items, unsigned count,
            const retropicoes_studio2_settings_t &working,
            const retropicoes_settings_menu_state_t &state) {
    retropicoes_settings_item_view_t rows[14]{};
    char values[14][8]{};
    uint16_t preview[RETROPICOES_STUDIO2_THEME_COLOR_COUNT]{};
    for (unsigned i = 0; i < count; ++i)
        rows[i] = {label(items[i]), value(items[i], working, values[i], sizeof(values[i])),
                   help(items[i])};
    for (unsigned i = 0; i < RETROPICOES_STUDIO2_THEME_COLOR_COUNT; ++i)
        preview[i] = retropicoes_studio2_theme_color(i);
    const auto *labels =
        retropicoes_menu_controller_labels(platform->controller_profile(platform->context));
    auto baseline = g_settings;
    if (retropicoes_studio2_frontend_can_start_tennis()) {
        retropicoes_studio2_input_debug_t input{};
        retropicoes_studio2_frontend_get_input_debug(&input);
        baseline.keypad_player = input.player;
        baseline.keypad_bank = input.bank;
    }
    const retropicoes_settings_view_t view{rows,
                                           count,
                                           labels,
                                           preview,
                                           RETROPICOES_STUDIO2_THEME_COLOR_COUNT,
                                           working.foreground_color,
                                           working.background_color,
                                           std::memcmp(&working, &baseline, sizeof(working)) !=
                                               0};
    compat_framebuffer_t *fb = compat_framebuffer();
    if (fb == nullptr)
        return;
    retropicoes_menu_surface_t surface{fb->pixels, fb->stride, fb->width, fb->height};
    retropicoes_menu_renderer_t renderer{};
    platform->get_renderer(platform->context, &renderer);
    (void)retropicoes_settings_menu_render(&renderer, &surface, &state, &view);
    compat_present();
}
} // namespace

extern "C" void retropicoes_studio2_settings_ui_init(void) { load(); }
extern "C" const retropicoes_studio2_settings_t *retropicoes_studio2_settings_ui_current(void) {
    return &g_settings;
}
extern "C" retropicoes_studio2_menu_request_t
retropicoes_studio2_settings_ui_run(int in_game, const retropicoes_settings_platform_t *platform) {
    struct Context {
        const retropicoes_settings_platform_t *platform;
        Item items[14];
        unsigned count;
        retropicoes_studio2_settings_t working;
        retropicoes_studio2_menu_request_t request;
        retropicoes_settings_home_gate_t gate;
    } context{platform, {}, 0u, g_settings, RETROPICOES_STUDIO2_MENU_REQUEST_RESUME, {}};
    if (in_game && retropicoes_studio2_frontend_can_start_tennis()) {
        retropicoes_studio2_input_debug_t input{};
        retropicoes_studio2_frontend_get_input_debug(&input);
        context.working.keypad_player = input.player;
        context.working.keypad_bank = input.bank;
    }
    retropicoes_settings_home_gate_init(&context.gate, 0);
    context.count = build_items(in_game != 0, context.items);
    const retropicoes_settings_host_t host{
        &context,
        platform,
        context.count,
        0u,
        nullptr,
        [](void *opaque, const retropicoes_settings_menu_state_t *state) {
            auto *ctx = static_cast<Context *>(opaque);
            render(ctx->platform, ctx->items, ctx->count, ctx->working, *state);
        },
        [](void *) { compat_poll_input(); },
        [](void *, unsigned ms) { compat_delay_ms(ms); },
        [](void *opaque, retropicoes_ui_actions_t actions) -> retropicoes_ui_actions_t {
            auto *ctx = static_cast<Context *>(opaque);
            bool held = false;
            for (unsigned k = 0; k < COMPAT_KEY_COUNT; k++)
                held |= compat_key_down(static_cast<compat_key_t>(k)) != 0;
            return retropicoes_settings_home_gate_filter(&ctx->gate, actions, held,
                                                         compat_ticks_ms(), 100u);
        },
        [](void *opaque, retropicoes_settings_event_t event) -> int {
            auto *ctx = static_cast<Context *>(opaque);
            if (event.type == RETROPICOES_SETTINGS_EVENT_CHANGE) {
                change(ctx->items[event.item], &ctx->working, event.delta);
            } else if (event.type == RETROPICOES_SETTINGS_EVENT_DEFAULT) {
                retropicoes_studio2_settings_defaults(&ctx->working);
            } else if (event.type == RETROPICOES_SETTINGS_EVENT_SAVE) {
                if (!save(ctx->working)) {
                    std::printf("STUDIO2 SETTINGS: SAVE FAILED\n");
                    return 1;
                }
                g_settings = ctx->working;
                apply(g_settings);
                return 0;
            } else if (event.type == RETROPICOES_SETTINGS_EVENT_CANCEL) {
                return 0;
            } else if (event.type == RETROPICOES_SETTINGS_EVENT_ACTIVATE) {
                switch (ctx->items[event.item]) {
                case QUIT:
                    ctx->request = RETROPICOES_STUDIO2_MENU_REQUEST_ROM_LIST;
                    return 0;
                case RESET:
                    if (!confirm(ctx->platform, "RESET GAME?"))
                        break;
                    ctx->request = RETROPICOES_STUDIO2_MENU_REQUEST_RESET;
                    return 0;
                case LOADER:
                    if (!confirm(ctx->platform, "RETURN TO LOADER?"))
                        break;
                    ctx->request = RETROPICOES_STUDIO2_MENU_REQUEST_RETURN_LOADER;
                    return 0;

                case CONTROLLER:
                    controls(ctx->platform);
                    break;
                case TENNIS: {
                    if (!confirm(ctx->platform, "RESTART TENNIS?"))
                        break;
                    if (!retropicoes_studio2_frontend_start_tennis()) {
                        return 0;
                    }
                    break;
                }
                default:
                    break;
                }
            }
            return 1;
        }};
    if (!retropicoes_settings_host_run(&host))
        std::printf("STUDIO2 SETTINGS: common host rejected adapter\n");
    return context.request;
}
