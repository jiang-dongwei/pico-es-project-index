#include "settings_adapter.h"

#include <string.h>

#include "compat.h"
#include "input_backend.h"
#include "settings_host.h"

typedef struct settings_context {
    space_invaders_settings_action_t action;
    retropicoes_settings_item_view_t rows[6];
    retropicoes_settings_platform_t platform;
} settings_context_t;

static void get_renderer(void *context, retropicoes_menu_renderer_t *renderer)
{
    (void)context;
    *renderer = retropicoes_menu_nintendo_renderer();
}

static void noop(void *context) { (void)context; }

static retropicoes_controller_profile_t controller_profile(void *context)
{
    (void)context;
    return RETROPICOES_CONTROLLER_XBOX;
}

static retropicoes_ui_actions_t read_actions(
    void *context, retropicoes_menu_repeat_t *repeat)
{
    retropicoes_ui_actions_t held = 0u;
    retropicoes_ui_actions_t pressed = 0u;
    (void)context;
    if (compat_key_down(COMPAT_KEY_UP)) held |= RETROPICOES_UI_UP;
    if (compat_key_down(COMPAT_KEY_DOWN)) held |= RETROPICOES_UI_DOWN;
    if (compat_key_pressed(COMPAT_KEY_UP)) pressed |= RETROPICOES_UI_UP;
    if (compat_key_pressed(COMPAT_KEY_DOWN)) pressed |= RETROPICOES_UI_DOWN;
    pressed |= retropicoes_menu_face_actions_from_compat(
        RETROPICOES_CONTROLLER_XBOX,
        compat_key_pressed(COMPAT_KEY_ACTION),
        compat_key_pressed(COMPAT_KEY_JUMP));
    if (retropicoes_input_menu_pressed()) pressed |= RETROPICOES_UI_MENU;
    return (pressed & ~(RETROPICOES_UI_UP | RETROPICOES_UI_DOWN)) |
        retropicoes_menu_repeat_update(repeat, held, pressed,
                                       compat_ticks_ms());
}

static void render(void *context, const retropicoes_settings_menu_state_t *state)
{
    settings_context_t *ui = (settings_context_t *)context;
    compat_framebuffer_t *fb = compat_framebuffer();
    retropicoes_menu_surface_t surface;
    retropicoes_menu_renderer_t renderer;
    retropicoes_settings_view_t view;
    if (fb == NULL || fb->pixels == NULL) return;
    surface.pixels = fb->pixels;
    surface.stride = fb->stride;
    surface.width = fb->width;
    surface.height = fb->height;
    renderer = retropicoes_menu_nintendo_renderer();
    memset(&view, 0, sizeof(view));
    view.items = ui->rows;
    view.item_count = 6u;
    view.labels = retropicoes_menu_controller_labels(
        RETROPICOES_CONTROLLER_XBOX);
    (void)retropicoes_settings_menu_render(&renderer, &surface, state, &view);
    compat_present();
}

static void poll_input(void *context) { (void)context; compat_poll_input(); }
static void delay_ms(void *context, unsigned ms)
{
    (void)context;
    compat_delay_ms(ms);
}

static int on_event(void *context, retropicoes_settings_event_t event)
{
    settings_context_t *ui = (settings_context_t *)context;
    if (event.type == RETROPICOES_SETTINGS_EVENT_ACTIVATE) {
        ui->action = (space_invaders_settings_action_t)event.item;
        return 0;
    }
    if (event.type == RETROPICOES_SETTINGS_EVENT_CANCEL ||
        event.type == RETROPICOES_SETTINGS_EVENT_SAVE)
        return 0;
    return 1;
}

space_invaders_settings_action_t space_invaders_settings_run(void)
{
    settings_context_t ui;
    retropicoes_settings_host_t host;
    memset(&ui, 0, sizeof(ui));
    ui.rows[0].label = "Resume game";
    ui.rows[0].help = "CONTINUE PLAYING";
    ui.rows[1].label = "Save state";
    ui.rows[1].help = "SAVE THE CURRENT GAME TO SD";
    ui.rows[2].label = "Load state";
    ui.rows[2].help = "RESTORE A PREVIOUSLY SAVED GAME";
    ui.rows[3].label = "Reset game";
    ui.rows[3].help = "RESTART WITHOUT LOADING SAVE";
    ui.rows[4].label = "Quit game";
    ui.rows[4].help = "SAVE AND RETURN TO GAME LIST";
    ui.rows[5].label = "Return to Loader";
    ui.rows[5].help = "SAVE AND SELECT AN EMULATOR";
    ui.platform.context = &ui;
    ui.platform.get_renderer = get_renderer;
    ui.platform.reset_owner = noop;
    ui.platform.update_owner = noop;
    ui.platform.controller_profile = controller_profile;
    ui.platform.read_actions = read_actions;
    memset(&host, 0, sizeof(host));
    host.context = &ui;
    host.platform = &ui.platform;
    host.item_count = 6u;
    host.page_rows = 6u;
    host.render = render;
    host.poll_input = poll_input;
    host.delay_ms = delay_ms;
    host.on_event = on_event;
    (void)retropicoes_settings_host_run(&host);
    return ui.action;
}
