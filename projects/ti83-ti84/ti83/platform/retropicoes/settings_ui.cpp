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
constexpr uint32_t kMagic = 0x53333854u;
constexpr uint16_t kVersion = 1u;
constexpr const char *kPath = "/settings_TI83.dat";
retropicoes_ti83_settings_t g_settings{};
struct SettingsFile { uint32_t magic; uint16_t version; uint16_t size;
                      retropicoes_ti83_settings_t settings; };
using Item = retropicoes_settings_option_id_t;
constexpr Item QUIT = RETROPICOES_SETTINGS_OPTION_QUIT;
constexpr Item RESET = RETROPICOES_SETTINGS_OPTION_RESET;
constexpr Item LOADER = RETROPICOES_SETTINGS_OPTION_RETURN_LOADER;
constexpr Item SCANLINES = RETROPICOES_SETTINGS_OPTION_SCANLINES;
constexpr Item FPS = RETROPICOES_SETTINGS_OPTION_FPS;
constexpr Item AUDIO = RETROPICOES_SETTINGS_OPTION_AUDIO;
constexpr Item FOREGROUND = RETROPICOES_SETTINGS_OPTION_MENU_FOREGROUND;
constexpr Item BACKGROUND = RETROPICOES_SETTINGS_OPTION_MENU_BACKGROUND;
constexpr Item CONTROLLER = RETROPICOES_SETTINGS_OPTION_CONTROLLER_TEST;
constexpr Item MODE = static_cast<Item>(RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE);
unsigned build_items(bool in_game, Item *items)
{
    uint32_t capabilities = RETROPICOES_SETTINGS_CAP_RETURN_LOADER |
        RETROPICOES_SETTINGS_CAP_SCANLINES | RETROPICOES_SETTINGS_CAP_FPS |
        RETROPICOES_SETTINGS_CAP_MENU_COLORS |
        RETROPICOES_SETTINGS_CAP_CONTROLLER_TEST;
    const Item custom[] = {MODE};
    if (in_game) capabilities |= RETROPICOES_SETTINGS_CAP_QUIT | RETROPICOES_SETTINGS_CAP_RESET;
    return static_cast<unsigned>(retropicoes_settings_build_option_order(
        items, 14u, capabilities, custom, 1u));
}
const char *label(Item item)
{
    const char *common = retropicoes_settings_common_label(item);
    if (common != nullptr) return common;
    return "Virtual Keypad";
}
const char *value(Item item, const retropicoes_ti83_settings_t &s,
                  char *, size_t)
{
    switch (item) {
    case AUDIO: return s.audio_enabled ? "ON" : "OFF";
    case SCANLINES: return s.scanlines_enabled ? "ON" : "OFF";
    case FPS: return s.fps_overlay_enabled ? "ON" : "OFF";
    case MODE: return s.controller_mode ? "ON" : "OFF";
    case FOREGROUND: return retropicoes_ti83_theme_color_name(s.foreground_color);
    case BACKGROUND: return retropicoes_ti83_theme_color_name(s.background_color);
    default: return "";
    }
}
const char *help(Item item)
{
    const char *common = retropicoes_settings_common_help(item);
    if (common != nullptr) return common;
    return "START:KEYPAD; GAME A:2ND B:ALPHA";
}
void change(Item item, retropicoes_ti83_settings_t *s, int delta)
{
    switch (item) {
    case AUDIO: s->audio_enabled ^= 1u; break;
    case SCANLINES: s->scanlines_enabled ^= 1u; break;
    case FPS: s->fps_overlay_enabled ^= 1u; break;
    case MODE: s->controller_mode = static_cast<uint8_t>((s->controller_mode + 2 + delta) % 2); break;
    case FOREGROUND: s->foreground_color = static_cast<uint8_t>((s->foreground_color + RETROPICOES_TI83_THEME_COLOR_COUNT + delta) % RETROPICOES_TI83_THEME_COLOR_COUNT); break;
    case BACKGROUND: s->background_color = static_cast<uint8_t>((s->background_color + RETROPICOES_TI83_THEME_COLOR_COUNT + delta) % RETROPICOES_TI83_THEME_COLOR_COUNT); break;
    default: break;
    }
    (void)retropicoes_ti83_settings_sanitize(s);
}
void apply(const retropicoes_ti83_settings_t &s)
{
    retropicoes_ti83_frontend_config_t config{};
    config.audio_enabled = s.audio_enabled; config.scanlines_enabled = s.scanlines_enabled;
    config.fps_overlay_enabled = s.fps_overlay_enabled; config.controller_mode = s.controller_mode;
    retropicoes_ti83_frontend_set_config(&config);
}
bool read_settings(const char *path, SettingsFile *stored)
{
    compat_file_t *file=compat_file_open(path,"rb");
    if (!file) return false;
    const bool valid=compat_file_size(file)==sizeof(*stored) &&
        compat_file_read(stored,1,sizeof(*stored),file)==sizeof(*stored) &&
        stored->magic==kMagic && stored->version==kVersion && stored->size==sizeof(stored->settings);
    compat_file_close(file); return valid;
}
void load()
{
    SettingsFile stored{};
    retropicoes_ti83_settings_defaults(&g_settings);
    if (read_settings(kPath,&stored) || read_settings("/settings_TI83.bak",&stored))
        g_settings=stored.settings;
    (void)retropicoes_ti83_settings_sanitize(&g_settings);
    apply(g_settings);
}
bool write_settings(const char *path, const retropicoes_ti83_settings_t &settings)
{
    const SettingsFile stored{kMagic,kVersion,sizeof(settings),settings};
    compat_file_t *file=compat_file_open(path,"wb");
    if (!file) return false;
    const bool written=compat_file_write(&stored,1,sizeof(stored),file)==sizeof(stored);
    const bool closed=compat_file_close(file)==0;
    return written && closed;
}
bool save(const retropicoes_ti83_settings_t &settings)
{
    if (!write_settings("/settings_TI83.bak",g_settings)) return false;
    return write_settings(kPath,settings);
}
void fill_white(retropicoes_menu_surface_t *surface)
{
    for (size_t y = 0; y < surface->height; ++y) for (size_t x = 0; x < surface->width; ++x) surface->pixels[y * surface->stride + x] = 0xffffu;
}
void controls(const retropicoes_settings_platform_t *platform)
{
    retropicoes_menu_renderer_t renderer{}; platform->get_renderer(platform->context, &renderer);
    do { compat_poll_input(); compat_delay_ms(16u); } while (compat_key_down(COMPAT_KEY_ACTION) || compat_key_down(COMPAT_KEY_JUMP));
    retropicoes_menu_repeat_t repeat{};
    for (;;) {
        compat_poll_input(); platform->update_owner(platform->context);
        retropicoes_input_stats_t stats{}; retropicoes_input_get_stats(&stats);
        compat_framebuffer_t *fb = compat_framebuffer(); if (fb == nullptr) return;
        retropicoes_menu_surface_t surface{fb->pixels, fb->stride, fb->width, fb->height}; fill_white(&surface);
        char line[64]; renderer.draw_text(&surface, 94, 8, "-- CONTROLLER TEST --", 0, 1, 26, renderer.text_context);
        for (unsigned source = 0; source < 4; ++source) {
            std::snprintf(line, sizeof(line), "%s%u: %08X", source < 2 ? "USB" : "NES", source % 2 + 1, static_cast<unsigned>(stats.source_masks[source]));
            renderer.draw_text(&surface, 10, 45 + static_cast<int>(source) * 24, line, 0, 1, 48, renderer.text_context);
        }
        renderer.draw_text(&surface, 10, 155, "DPAD: MOVE  A: ENTER  B: CLEAR", 0, 1, 48, renderer.text_context);
        renderer.draw_text(&surface, 10, 175, "START: KEYPAD; B: 2ND IN KEYPAD", 0, 1, 48, renderer.text_context);
        const auto *labels = retropicoes_menu_controller_labels(platform->controller_profile(platform->context));
        std::snprintf(line, sizeof(line), "PRESS %s TO GO BACK", labels->cancel);
        renderer.draw_text(&surface, 100, 225, line, 0, 1, 36, renderer.text_context); compat_present();
        if ((platform->read_actions(platform->context, &repeat) & (RETROPICOES_UI_CANCEL | RETROPICOES_UI_MENU)) != 0u) return;
        compat_delay_ms(16u);
    }
}
void save_error(const retropicoes_settings_platform_t *platform)
{
    const char *rows[]={"SD WRITE FAILED", "PREVIOUS SETTINGS KEPT"};
    retropicoes_menu_repeat_t repeat{};
    for (;;) {
        compat_framebuffer_t *fb=compat_framebuffer(); if (!fb) return;
        retropicoes_menu_surface_t surface{fb->pixels,fb->stride,fb->width,fb->height};
        retropicoes_menu_renderer_t renderer{};platform->get_renderer(platform->context,&renderer);
        retropicoes_menu_view_t view{"Settings",rows,2,0,0,2,
            retropicoes_menu_controller_labels(platform->controller_profile(platform->context)),nullptr};
        retropicoes_menu_render(&renderer,&surface,&view);compat_present();
        compat_poll_input();platform->update_owner(platform->context);
        if (platform->read_actions(platform->context,&repeat)&
            (RETROPICOES_UI_CONFIRM|RETROPICOES_UI_CANCEL|RETROPICOES_UI_MENU))return;
        compat_delay_ms(16);
    }
}
bool confirm(const retropicoes_settings_platform_t *platform, Item item)
{
    retropicoes_system_menu_state_t state{};
    state.open = 1; state.item_count = 1;
    state.confirmation_active = 1;
    state.confirmation_item = item == RESET ? RETROPICOES_MENU_ITEM_RESET : RETROPICOES_MENU_ITEM_RETURN_LOADER;
    state.items[0] = state.confirmation_item;
    retropicoes_menu_list_init(&state.list, 1, 1);
    retropicoes_menu_repeat_t repeat{};
    for (;;) {
        compat_framebuffer_t *fb = compat_framebuffer(); if (!fb) return false;
        retropicoes_menu_surface_t surface{fb->pixels, fb->stride, fb->width, fb->height};
        retropicoes_menu_renderer_t renderer{}; platform->get_renderer(platform->context, &renderer);
        retropicoes_system_menu_render(&renderer, &surface, &state,
            retropicoes_menu_controller_labels(platform->controller_profile(platform->context)), "Settings");
        compat_present(); compat_poll_input(); platform->update_owner(platform->context);
        const auto event = retropicoes_system_menu_update(&state, platform->read_actions(platform->context, &repeat));
        if (event.type == RETROPICOES_MENU_EVENT_ACTIVATE) return true;
        if (!state.confirmation_active) return false;
        compat_delay_ms(16);
    }
}
void render(const retropicoes_settings_platform_t *platform, const Item *items, unsigned count, const retropicoes_ti83_settings_t &working, const retropicoes_settings_menu_state_t &state)
{
    retropicoes_settings_item_view_t rows[14]{}; char values[14][8]{}; uint16_t preview[RETROPICOES_TI83_THEME_COLOR_COUNT]{};
    for (unsigned i = 0; i < count; ++i) rows[i] = {label(items[i]), value(items[i], working, values[i], sizeof(values[i])), help(items[i])};
    for (unsigned i = 0; i < RETROPICOES_TI83_THEME_COLOR_COUNT; ++i) preview[i] = retropicoes_ti83_theme_color(i);
    const auto *labels = retropicoes_menu_controller_labels(platform->controller_profile(platform->context));
    const retropicoes_settings_view_t view{rows, count, labels, preview, RETROPICOES_TI83_THEME_COLOR_COUNT, working.foreground_color, working.background_color, std::memcmp(&working, &g_settings, sizeof(working)) != 0};
    compat_framebuffer_t *fb = compat_framebuffer(); if (fb == nullptr) return;
    retropicoes_menu_surface_t surface{fb->pixels, fb->stride, fb->width, fb->height}; retropicoes_menu_renderer_t renderer{};
    platform->get_renderer(platform->context, &renderer); (void)retropicoes_settings_menu_render(&renderer, &surface, &state, &view); compat_present();
}
}  // namespace

extern "C" void retropicoes_ti83_settings_ui_init(void) { load(); }
extern "C" const retropicoes_ti83_settings_t *retropicoes_ti83_settings_ui_current(void) { return &g_settings; }
extern "C" retropicoes_ti83_menu_request_t retropicoes_ti83_settings_ui_run(int in_game, const retropicoes_settings_platform_t *platform)
{
    struct Context {
        const retropicoes_settings_platform_t *platform;
        Item items[14];
        unsigned count;
        retropicoes_ti83_settings_t working;
        retropicoes_ti83_menu_request_t request;
        bool save_failed;
    } context{platform, {}, 0u, g_settings,
              RETROPICOES_TI83_MENU_REQUEST_RESUME, false};
    context.count = build_items(in_game != 0, context.items);
    const retropicoes_settings_host_t host{
        &context, platform, context.count, 0u,
        nullptr,
        [](void *opaque, const retropicoes_settings_menu_state_t *state) {
            auto *ctx = static_cast<Context *>(opaque);
            render(ctx->platform, ctx->items, ctx->count, ctx->working, *state);
        },
        [](void *) { compat_poll_input(); },
        [](void *, unsigned ms) { compat_delay_ms(ms); },
        nullptr,
        [](void *opaque, retropicoes_settings_event_t event) -> int {
            auto *ctx = static_cast<Context *>(opaque);
            if (event.type == RETROPICOES_SETTINGS_EVENT_CHANGE) {
                change(ctx->items[event.item], &ctx->working, event.delta);
            } else if (event.type == RETROPICOES_SETTINGS_EVENT_DEFAULT) {
                retropicoes_ti83_settings_defaults(&ctx->working);
            } else if (event.type == RETROPICOES_SETTINGS_EVENT_SAVE) {
                if (!save(ctx->working)) {
                    std::printf("TI83 SETTINGS: save failed; settings unchanged\n");
                    ctx->save_failed=true;
                    return 0;
                }
                g_settings = ctx->working;
                apply(g_settings);
                return 0;
            } else if (event.type == RETROPICOES_SETTINGS_EVENT_CANCEL) {
                return 0;
            } else if (event.type == RETROPICOES_SETTINGS_EVENT_ACTIVATE) {
                switch (ctx->items[event.item]) {
                case QUIT:
                    ctx->request = RETROPICOES_TI83_MENU_REQUEST_ROM_LIST;
                    return 0;
                case RESET:
                    if (!confirm(ctx->platform, RESET)) break;
                    ctx->request = RETROPICOES_TI83_MENU_REQUEST_RESET;
                    return 0;
                case LOADER:
                    if (!confirm(ctx->platform, LOADER)) break;
                    ctx->request = RETROPICOES_TI83_MENU_REQUEST_RETURN_LOADER;
                    return 0;

                case CONTROLLER: controls(ctx->platform); break;
                default: break;
                }
            }
            return 1;
        }
    };
    if (!retropicoes_settings_host_run(&host))
        std::printf("TI83 SETTINGS: common host rejected adapter\n");
    if (context.save_failed) save_error(platform);
    return context.request;
}
