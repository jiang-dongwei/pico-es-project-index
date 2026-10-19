#ifndef RETROPICOES_COMMON_ROM_BROWSER_H
#define RETROPICOES_COMMON_ROM_BROWSER_H

#include <stddef.h>
#include <stdint.h>

#include "menu_render.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RETROPICOES_ROM_SOURCE_VERSION 1u
#define RETROPICOES_ROM_BROWSER_DEFAULT_PAGE_ROWS 18u
#define RETROPICOES_ROM_BROWSER_DEMO_INDEX ((size_t)-1)

typedef enum retropicoes_rom_item_kind {
    RETROPICOES_ROM_ITEM_ROM = 0,
    RETROPICOES_ROM_ITEM_DIRECTORY,
    RETROPICOES_ROM_ITEM_DEMO
} retropicoes_rom_item_kind_t;

typedef struct retropicoes_rom_item_view {
    const char *label;
    retropicoes_rom_item_kind_t kind;
} retropicoes_rom_item_view_t;

typedef size_t (*retropicoes_rom_count_fn)(void *context);
typedef int (*retropicoes_rom_item_fn)(void *context,
                                      size_t index,
                                      retropicoes_rom_item_view_t *item);
typedef const char *(*retropicoes_rom_path_fn)(void *context);

/*
 * A source owns directory scanning, filtering and stable item names. The
 * common browser owns selection, paging, the empty-directory demo item,
 * button semantics and rendering.
 */
typedef struct retropicoes_rom_source {
    uint32_t version;
    size_t struct_size;
    void *context;
    const char *title;
    const char *empty_message;
    const char *demo_label;
    retropicoes_rom_count_fn count;
    retropicoes_rom_item_fn item;
    retropicoes_rom_path_fn path;
} retropicoes_rom_source_t;

typedef enum retropicoes_rom_browser_event_type {
    RETROPICOES_ROM_BROWSER_EVENT_NONE = 0,
    RETROPICOES_ROM_BROWSER_EVENT_REDRAW,
    RETROPICOES_ROM_BROWSER_EVENT_OPEN,
    RETROPICOES_ROM_BROWSER_EVENT_BACK,
    RETROPICOES_ROM_BROWSER_EVENT_SETTINGS,
    RETROPICOES_ROM_BROWSER_EVENT_ERROR
} retropicoes_rom_browser_event_type_t;

typedef struct retropicoes_rom_browser_event {
    retropicoes_rom_browser_event_type_t type;
    size_t source_index;
    retropicoes_rom_item_kind_t item_kind;
} retropicoes_rom_browser_event_t;

typedef struct retropicoes_rom_browser {
    retropicoes_menu_list_state_t list;
    retropicoes_rom_source_t source;
    size_t source_count;
    int showing_demo;
    int open;
} retropicoes_rom_browser_t;

int retropicoes_rom_source_valid(const retropicoes_rom_source_t *source);
int retropicoes_rom_browser_open(retropicoes_rom_browser_t *browser,
                                 const retropicoes_rom_source_t *source,
                                 size_t page_rows);
int retropicoes_rom_browser_refresh(retropicoes_rom_browser_t *browser);
int retropicoes_rom_browser_item(const retropicoes_rom_browser_t *browser,
                                 size_t visible_index,
                                 retropicoes_rom_item_view_t *item,
                                 size_t *source_index);
retropicoes_rom_browser_event_t
retropicoes_rom_browser_update(retropicoes_rom_browser_t *browser,
                               retropicoes_ui_actions_t actions);
int retropicoes_rom_browser_render(
    const retropicoes_rom_browser_t *browser,
    const retropicoes_menu_renderer_t *renderer,
    retropicoes_menu_surface_t *surface,
    const retropicoes_controller_labels_t *labels);

#ifdef __cplusplus
}
#endif

#endif
