#ifndef RETROPICOES_COMMON_SETTINGS_HOST_H
#define RETROPICOES_COMMON_SETTINGS_HOST_H

#include <stddef.h>
#include "settings_menu.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*retropicoes_settings_host_event_fn)(
    void *context, retropicoes_settings_event_t event);

/* Suppress the opening Home report until the physical buttons have stayed
 * neutral long enough. A fresh Home report is then passed through unchanged. */
typedef struct retropicoes_settings_home_gate {
    uint32_t neutral_since_ms;
    unsigned neutral_started;
    unsigned armed;
} retropicoes_settings_home_gate_t;

void retropicoes_settings_home_gate_init(
    retropicoes_settings_home_gate_t *gate, int initially_armed);
retropicoes_ui_actions_t retropicoes_settings_home_gate_filter(
    retropicoes_settings_home_gate_t *gate,
    retropicoes_ui_actions_t actions,
    int physical_buttons_down,
    uint32_t now_ms,
    uint32_t stable_release_ms);

/* Adapter for upstream character-cell Settings pages. The public host owns
 * their list/action navigation while core-specific option handlers and
 * rendering remain callbacks during migration. New cores use run() below. */
typedef struct retropicoes_settings_legacy_nav {
    int selected;
    int first_visible;
    int on_action_row;
    int action;
} retropicoes_settings_legacy_nav_t;

void retropicoes_settings_host_legacy_move(
    retropicoes_settings_legacy_nav_t *nav,
    size_t item_count,
    size_t page_rows,
    retropicoes_ui_actions_t direction);

typedef struct retropicoes_settings_host {
    void *context;
    const retropicoes_settings_platform_t *platform;
    size_t item_count;
    size_t page_rows;
    /* Optional frame boundary hook. Called before touching the menu surface. */
    void (*begin_frame)(void *context);
    void (*render)(void *context,
                   const retropicoes_settings_menu_state_t *state);
    void (*poll_input)(void *context);
    void (*delay_ms)(void *context, unsigned milliseconds);
    /* Optional input gate for a still-held Menu/Home opening report. */
    retropicoes_ui_actions_t (*filter_actions)(
        void *context, retropicoes_ui_actions_t actions);
    retropicoes_settings_host_event_fn on_event;
} retropicoes_settings_host_t;

/* The host owns navigation, input polling and the Settings loop.
 * on_event returns nonzero to continue, zero to close the page. */
int retropicoes_settings_host_run(const retropicoes_settings_host_t *host);

#ifdef __cplusplus
}

/* Upstream char-cell renderer adapter: the public host drives each frame;
 * the callback owns only core-specific drawing and option side effects. */
template <class Frame>
void retropicoes_settings_host_run_legacy(Frame frame)
{
    while (frame()) {}
}
#endif

#endif
