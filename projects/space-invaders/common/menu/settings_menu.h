#ifndef RETROPICOES_COMMON_SETTINGS_MENU_H
#define RETROPICOES_COMMON_SETTINGS_MENU_H

#include <stddef.h>
#include <stdint.h>

#include "menu_render.h"
#include "settings_labels.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RETROPICOES_SETTINGS_DEFAULT_PAGE_ROWS 12u
#define RETROPICOES_SETTINGS_ACTION_COUNT 3u

typedef enum retropicoes_settings_option_id {
    RETROPICOES_SETTINGS_OPTION_QUIT = 0,
    RETROPICOES_SETTINGS_OPTION_RESET,
    RETROPICOES_SETTINGS_OPTION_RETURN_LOADER,
    RETROPICOES_SETTINGS_OPTION_SAVE_LOAD,
    RETROPICOES_SETTINGS_OPTION_SCREEN_MODE,
    RETROPICOES_SETTINGS_OPTION_SCANLINES,
    RETROPICOES_SETTINGS_OPTION_FPS,
    RETROPICOES_SETTINGS_OPTION_AUDIO,
    RETROPICOES_SETTINGS_OPTION_MENU_FOREGROUND,
    RETROPICOES_SETTINGS_OPTION_MENU_BACKGROUND,
    RETROPICOES_SETTINGS_OPTION_CONTROLLER_TEST,
    RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE = 0x100
} retropicoes_settings_option_id_t;

enum {
    RETROPICOES_SETTINGS_CAP_QUIT = 1u << 0,
    RETROPICOES_SETTINGS_CAP_RESET = 1u << 1,
    RETROPICOES_SETTINGS_CAP_RETURN_LOADER = 1u << 2,
    RETROPICOES_SETTINGS_CAP_SAVE_LOAD = 1u << 3,
    RETROPICOES_SETTINGS_CAP_SCREEN_MODE = 1u << 4,
    RETROPICOES_SETTINGS_CAP_SCANLINES = 1u << 5,
    RETROPICOES_SETTINGS_CAP_FPS = 1u << 6,
    RETROPICOES_SETTINGS_CAP_AUDIO = 1u << 7,
    RETROPICOES_SETTINGS_CAP_MENU_COLORS = 1u << 8,
    RETROPICOES_SETTINGS_CAP_CONTROLLER_TEST = 1u << 9
};

typedef enum retropicoes_settings_action {
    RETROPICOES_SETTINGS_ACTION_SAVE = 0,
    RETROPICOES_SETTINGS_ACTION_CANCEL,
    RETROPICOES_SETTINGS_ACTION_DEFAULT
} retropicoes_settings_action_t;

typedef enum retropicoes_settings_event_type {
    RETROPICOES_SETTINGS_EVENT_NONE = 0,
    RETROPICOES_SETTINGS_EVENT_REDRAW,
    RETROPICOES_SETTINGS_EVENT_CHANGE,
    RETROPICOES_SETTINGS_EVENT_ACTIVATE,
    RETROPICOES_SETTINGS_EVENT_SAVE,
    RETROPICOES_SETTINGS_EVENT_CANCEL,
    RETROPICOES_SETTINGS_EVENT_DEFAULT
} retropicoes_settings_event_type_t;

typedef struct retropicoes_settings_menu_state {
    retropicoes_menu_list_state_t list;
    size_t action;
    int on_action_row;
    int open;
} retropicoes_settings_menu_state_t;

size_t retropicoes_settings_build_option_order(
    retropicoes_settings_option_id_t *items,
    size_t capacity,
    uint32_t capabilities,
    const retropicoes_settings_option_id_t *custom_items,
    size_t custom_count);

const char *retropicoes_settings_common_label(
    retropicoes_settings_option_id_t item);
const char *retropicoes_settings_common_help(
    retropicoes_settings_option_id_t item);

typedef struct retropicoes_settings_event {
    retropicoes_settings_event_type_t type;
    size_t item;
    int delta;
} retropicoes_settings_event_t;

typedef struct retropicoes_settings_item_view {
    const char *label;
    const char *value;
    const char *help;
} retropicoes_settings_item_view_t;

typedef struct retropicoes_settings_view {
    const retropicoes_settings_item_view_t *items;
    size_t item_count;
    const retropicoes_controller_labels_t *labels;
    const uint16_t *preview_colors;
    size_t preview_color_count;
    size_t foreground_color;
    size_t background_color;
    int settings_changed;
} retropicoes_settings_view_t;

/* Platform bridge used by emulator-specific Settings adapters. The common
 * Settings implementation owns navigation and rendering; the main program
 * only supplies its existing font renderer and input ownership functions. */
typedef struct retropicoes_settings_platform {
    void *context;
    void (*get_renderer)(void *context,
                         retropicoes_menu_renderer_t *renderer);
    void (*reset_owner)(void *context);
    void (*update_owner)(void *context);
    retropicoes_controller_profile_t (*controller_profile)(void *context);
    retropicoes_ui_actions_t (*read_actions)(
        void *context, retropicoes_menu_repeat_t *repeat);
} retropicoes_settings_platform_t;

int retropicoes_settings_menu_open(retropicoes_settings_menu_state_t *state,
                                   size_t item_count,
                                   size_t page_rows);
retropicoes_settings_event_t retropicoes_settings_menu_update(
    retropicoes_settings_menu_state_t *state,
    retropicoes_ui_actions_t actions);
int retropicoes_settings_menu_render(
    const retropicoes_menu_renderer_t *renderer,
    retropicoes_menu_surface_t *surface,
    const retropicoes_settings_menu_state_t *state,
    const retropicoes_settings_view_t *view);

#ifdef __cplusplus
}
#endif

#endif

