#include "native_game_settings.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "compat.h"
#include "input_backend.h"
#include "settings_host.h"

typedef struct native_settings_context {
    retropicoes_native_game_action_t result;
    unsigned working_flags;
    retropicoes_settings_item_view_t rows[6];
    retropicoes_settings_platform_t platform;
} native_settings_context_t;

enum {
    NATIVE_SCANLINES = 1u << 0,
    NATIVE_FPS = 1u << 1,
    NATIVE_AUDIO = 1u << 2,
    NATIVE_DEFAULTS = NATIVE_AUDIO,
    NATIVE_MAGIC = 0x4e475331u
};

typedef struct native_settings_disk {
    uint32_t magic;
    uint32_t flags;
} native_settings_disk_t;

static _Atomic unsigned g_active_flags = NATIVE_DEFAULTS;
static char g_save_path[96];
static unsigned g_fps_frames;
static unsigned g_fps_x10;
static uint32_t g_fps_start_ms;

void retropicoes_native_game_settings_init(const char *save_path)
{
    compat_file_t *file;
    native_settings_disk_t disk;
    atomic_store(&g_active_flags, NATIVE_DEFAULTS);
    g_save_path[0] = '\0';
    g_fps_frames = 0u;
    g_fps_x10 = 0u;
    g_fps_start_ms = 0u;
    if (save_path == NULL || strlen(save_path) >= sizeof(g_save_path)) return;
    strcpy(g_save_path, save_path);
    file = compat_file_open(g_save_path, "rb");
    if (file == NULL) return;
    if (compat_file_read(&disk, sizeof(disk), 1u, file) == 1u &&
        disk.magic == NATIVE_MAGIC &&
        (disk.flags & ~(NATIVE_SCANLINES | NATIVE_FPS | NATIVE_AUDIO)) == 0u)
        atomic_store(&g_active_flags, disk.flags);
    (void)compat_file_close(file);
}

static void save_flags(unsigned flags)
{
    compat_file_t *file;
    native_settings_disk_t disk;
    if (g_save_path[0] == '\0') return;
    file = compat_file_open(g_save_path, "wb");
    if (file == NULL) {
        printf("NATIVE SETTINGS: cannot open %s for write\n", g_save_path);
        return;
    }
    disk.magic = NATIVE_MAGIC;
    disk.flags = flags;
    if (compat_file_write(&disk, sizeof(disk), 1u, file) != 1u)
        printf("NATIVE SETTINGS: write failed for %s\n", g_save_path);
    (void)compat_file_close(file);
}

int retropicoes_native_game_audio_enabled(void)
{
    return (atomic_load(&g_active_flags) & NATIVE_AUDIO) != 0u;
}

int retropicoes_native_game_scanlines_enabled(void)
{
    return (atomic_load(&g_active_flags) & NATIVE_SCANLINES) != 0u;
}

int retropicoes_native_game_fps_enabled(void)
{
    return (atomic_load(&g_active_flags) & NATIVE_FPS) != 0u;
}

void retropicoes_native_game_draw_fps(compat_framebuffer_t *framebuffer)
{
    uint32_t now = compat_ticks_ms();
    unsigned y;
    if (framebuffer == NULL || framebuffer->pixels == NULL) return;
    if (g_fps_start_ms == 0u) g_fps_start_ms = now;
    ++g_fps_frames;
    if ((uint32_t)(now - g_fps_start_ms) >= 1000u) {
        g_fps_x10 = (unsigned)((uint64_t)g_fps_frames * 10000u /
                               (uint32_t)(now - g_fps_start_ms));
        g_fps_frames = 0u;
        g_fps_start_ms = now;
    }
    /* Both native games center a 200-line picture in a 240-line surface. */
    for (y = 0u; y < 12u && y < framebuffer->height; ++y)
        memset(framebuffer->pixels + (size_t)y * framebuffer->stride, 0,
               framebuffer->width * sizeof(uint16_t));
    if (retropicoes_native_game_fps_enabled()) {
        retropicoes_menu_surface_t surface;
        retropicoes_menu_renderer_t renderer =
            retropicoes_menu_nintendo_renderer();
        char label[24];
        surface.pixels = framebuffer->pixels;
        surface.stride = framebuffer->stride;
        surface.width = framebuffer->width;
        surface.height = framebuffer->height;
        snprintf(label, sizeof(label), "FPS %u.%u", g_fps_x10 / 10u,
                 g_fps_x10 % 10u);
        renderer.draw_text(&surface, 4, 2, label, 0xffffu, 1u, 20u,
                           renderer.text_context);
    }
}

static void get_renderer(void *context, retropicoes_menu_renderer_t *renderer)
{
    (void)context;
    *renderer = retropicoes_menu_nintendo_renderer();
}

static void reset_owner(void *context) { (void)context; }
static void update_owner(void *context) { (void)context; }

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
    if (compat_key_down(COMPAT_KEY_LEFT)) held |= RETROPICOES_UI_LEFT;
    if (compat_key_down(COMPAT_KEY_RIGHT)) held |= RETROPICOES_UI_RIGHT;
    if (compat_key_pressed(COMPAT_KEY_UP)) pressed |= RETROPICOES_UI_UP;
    if (compat_key_pressed(COMPAT_KEY_DOWN)) pressed |= RETROPICOES_UI_DOWN;
    if (compat_key_pressed(COMPAT_KEY_LEFT)) pressed |= RETROPICOES_UI_LEFT;
    if (compat_key_pressed(COMPAT_KEY_RIGHT)) pressed |= RETROPICOES_UI_RIGHT;
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
    native_settings_context_t *ui = (native_settings_context_t *)context;
    compat_framebuffer_t *fb = compat_framebuffer();
    retropicoes_menu_surface_t surface;
    retropicoes_menu_renderer_t renderer;
    retropicoes_settings_view_t view;
    if (fb == NULL) return;
    surface.pixels = fb->pixels;
    surface.stride = fb->stride;
    surface.width = fb->width;
    surface.height = fb->height;
    ui->platform.get_renderer(ui, &renderer);
    memset(&view, 0, sizeof(view));
    view.items = ui->rows;
    view.item_count = 6u;
    ui->rows[0].value = (ui->working_flags & NATIVE_SCANLINES) ? "ON" : "OFF";
    ui->rows[1].value = (ui->working_flags & NATIVE_FPS) ? "ON" : "OFF";
    ui->rows[2].value = (ui->working_flags & NATIVE_AUDIO) ? "ON" : "OFF";
    view.settings_changed = ui->working_flags != atomic_load(&g_active_flags);
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
    native_settings_context_t *ui = (native_settings_context_t *)context;
    if (event.type == RETROPICOES_SETTINGS_EVENT_CHANGE ||
        event.type == RETROPICOES_SETTINGS_EVENT_ACTIVATE) {
        if (event.item < 3u) {
            ui->working_flags ^= 1u << event.item;
            return 1;
        }
    }
    if (event.type == RETROPICOES_SETTINGS_EVENT_ACTIVATE) {
        ui->result = event.item == 3u ? RETROPICOES_NATIVE_GAME_RESUME
                     : event.item == 4u ? RETROPICOES_NATIVE_GAME_QUIT
                                        : RETROPICOES_NATIVE_GAME_LOADER;
        return 0;
    }
    if (event.type == RETROPICOES_SETTINGS_EVENT_DEFAULT) {
        ui->working_flags = NATIVE_DEFAULTS;
        return 1;
    }
    if (event.type == RETROPICOES_SETTINGS_EVENT_SAVE) {
        atomic_store(&g_active_flags, ui->working_flags);
        save_flags(ui->working_flags);
        return 0;
    }
    if (event.type == RETROPICOES_SETTINGS_EVENT_CANCEL)
        return 0;
    return 1;
}

retropicoes_native_game_action_t retropicoes_native_game_settings_run(void)
{
    native_settings_context_t ui;
    retropicoes_settings_host_t host;
    memset(&ui, 0, sizeof(ui));
    ui.working_flags = atomic_load(&g_active_flags);
    ui.rows[0].label = "Scanlines";
    ui.rows[0].help = "DARKEN ALTERNATE DISPLAY LINES";
    ui.rows[1].label = "FPS Overlay";
    ui.rows[1].help = "SHOW MEASURED GAME PRESENT RATE";
    ui.rows[2].label = "Audio";
    ui.rows[2].help = "MUTE OR UNMUTE GAME SOUND";
    ui.rows[3].label = "Resume game";
    ui.rows[3].help = "CONTINUE PLAYING";
    ui.rows[4].label = "Quit game";
    ui.rows[4].help = "EXIT THIS NATIVE GAME";
    ui.rows[5].label = "Return to Loader";
    ui.rows[5].help = "RETURN TO EMULATOR SELECTION";
    ui.platform.context = &ui;
    ui.platform.get_renderer = get_renderer;
    ui.platform.reset_owner = reset_owner;
    ui.platform.update_owner = update_owner;
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
    return ui.result;
}
