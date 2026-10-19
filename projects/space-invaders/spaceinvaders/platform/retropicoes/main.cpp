/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <cstdio>
#include <cstring>

#include "hardware/uart.h"
#include "pico/time.h"

extern "C" {
#include "audio.h"
#include "audio_backend.h"
#include "compat.h"
#include "frontend.h"
#include "input_backend.h"
#include "perf_report.h"
#include "runtime.h"
#include "settings_adapter.h"
#include "state_adapter.h"
#include "video_backend.h"
#include "frontend_shell.h"
#include "rom_browser.h"
}

namespace {
constexpr unsigned kAudioRingSamples = 4096u;
constexpr unsigned kAudioPrefillSamples = 1024u;
constexpr unsigned kAudioFrameMax = 368u;
constexpr unsigned kStateCapacity = 9000u;
constexpr uint32_t kFrameBaseUs = 1000000u / 60u;
constexpr uint32_t kFrameRemainderUs = 1000000u % 60u;

space_invaders_frontend_t g_frontend;
retropicoes_frontend_shell_t g_shell;
retropicoes_rom_browser_t g_browser;
uint8_t g_rom_data[SPACE_INVADERS_ROM_COUNT][SPACE_INVADERS_ROM_CHIP_SIZE]
    __attribute__((aligned(4)));
compat_audio_ring_t g_audio_ring;
int16_t g_audio_ring_storage[kAudioRingSamples];
int16_t g_audio_frame[kAudioFrameMax];
uint8_t g_state_data[kStateCapacity];
uint32_t g_audio_underflow;
uint32_t g_audio_dropped;
uint32_t g_audio_fraction;
bool g_game_active;
bool g_skip_state_save;
uint32_t g_diag_keys_seen;
uint8_t g_diag_board_inputs_seen;
uint64_t g_diag_instructions;
uint32_t g_diag_previous_vram_hash;

char g_uart_buffer[512];
size_t g_uart_length;
size_t g_uart_position;

static const uint8_t kFont[36][7] = {
    {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14},{7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},{14,17,17,15,1,1,14}
};

void fill(compat_framebuffer_t *fb, int x, int y, int width, int height,
          uint16_t color)
{
    if (fb == nullptr) return;
    for (int py = y; py < y + height; ++py)
        for (int px = x; px < x + width; ++px)
            if (px >= 0 && py >= 0 && px < static_cast<int>(fb->width) &&
                py < static_cast<int>(fb->height))
                fb->pixels[static_cast<size_t>(py) * fb->stride + px] = color;
}

void character(compat_framebuffer_t *fb, int x, int y, char ch,
               uint16_t color, int scale)
{
    const uint8_t *glyph = nullptr;
    if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
    if (ch >= 'A' && ch <= 'Z') glyph = kFont[ch - 'A'];
    else if (ch >= '0' && ch <= '9') glyph = kFont[26 + ch - '0'];
    if (glyph != nullptr) {
        for (int row = 0; row < 7; ++row)
            for (int column = 0; column < 5; ++column)
                if ((glyph[row] & (1u << (4 - column))) != 0u)
                    fill(fb, x + column * scale, y + row * scale,
                         scale, scale, color);
    } else if (ch == '.') fill(fb, x + 2 * scale, y + 6 * scale,
                               scale, scale, color);
    else if (ch == '-') fill(fb, x, y + 3 * scale, 5 * scale, scale, color);
    else if (ch == '/')
        for (int row = 0; row < 7; ++row)
            fill(fb, x + (4 - row * 4 / 6) * scale, y + row * scale,
                 scale, scale, color);
}

void text(compat_framebuffer_t *fb, int x, int y, const char *value,
          uint16_t color, int scale, unsigned max_chars = 52u)
{
    unsigned count = 0u;
    while (value != nullptr && *value != '\0' && count++ < max_chars) {
        character(fb, x, y, *value++, color, scale);
        x += 6 * scale;
    }
}

void draw_message(const char *title, const char *line1, const char *line2)
{
    compat_framebuffer_t *fb = compat_framebuffer();
    if (fb == nullptr) return;
    std::memset(fb->pixels, 0,
                fb->stride * fb->height * sizeof(*fb->pixels));
    text(fb, 28, 58, title, 0xFD68, 2u, 24u);
    text(fb, 16, 112, line1, 0xFFFF, 1u, 48u);
    text(fb, 16, 136, line2, 0x7DFF, 1u, 48u);
    compat_present();
}

void show_stage(const char *stage, const char *detail)
{
    draw_message("SPACE INVADERS", stage, detail);
    std::printf("SPACEINVADERS LOAD: %s %s\r\n", stage, detail);
}

void audio_callback(int16_t *samples, size_t count, void *)
{
    const size_t read = compat_audio_ring_read(&g_audio_ring, samples, count);
    __atomic_fetch_add(&g_audio_underflow, static_cast<uint32_t>(count - read),
                       __ATOMIC_RELAXED);
}

bool read_exact(const char *path, uint8_t *data, size_t size,
                char *error, size_t error_capacity)
{
    compat_file_t *file = compat_file_open(path, "rb");
    if (file == nullptr) {
        std::snprintf(error, error_capacity, "MISSING %s", path);
        return false;
    }
    const long actual = compat_file_size(file);
    if (actual != static_cast<long>(size)) {
        std::snprintf(error, error_capacity, "BAD SIZE %s", path);
        compat_file_close(file);
        return false;
    }
    const bool ok = compat_file_read(data, 1u, size, file) == size;
    compat_file_close(file);
    if (!ok) std::snprintf(error, error_capacity, "READ FAILED %s", path);
    return ok;
}

bool load_roms(char *error, size_t error_capacity)
{
    space_invaders_rom_set_t roms = {};
    char path[96];
    for (unsigned index = 0u; index < SPACE_INVADERS_ROM_COUNT; ++index) {
        if (!space_invaders_rom_path(index, path, sizeof(path))) {
            std::snprintf(error, error_capacity, "ROM PATH %u FAILED",
                          index + 1u);
            return false;
        }
        char stage[32];
        std::snprintf(stage, sizeof(stage), "CHECK ROM %u OF 4", index + 1u);
        show_stage(stage, path);
        if (
            !read_exact(path, g_rom_data[index], sizeof(g_rom_data[index]),
                        error, error_capacity)) return false;
        roms.data[index] = g_rom_data[index];
        roms.size[index] = sizeof(g_rom_data[index]);
    }
    show_stage("CREATE BOARD CORE", "8080 1.9968 MHZ");
    const space_invaders_rom_result_t result =
        space_invaders_frontend_load_roms(&g_frontend, &roms);
    if (result.status != SPACE_INVADERS_ROM_OK) {
        space_invaders_rom_error_text(&result, error, error_capacity);
        return false;
    }
    return true;
}

bool save_state()
{
    if (!g_game_active) return false;
    const size_t needed = space_invaders_state_size();
    size_t written = 0u;
    char path[96];
    if (needed > sizeof(g_state_data) ||
        !space_invaders_state_path(path, sizeof(path)) ||
        space_invaders_state_save(&g_frontend.board, g_state_data,
                                  sizeof(g_state_data), &written) !=
            SPACE_INVADERS_STATE_OK ||
        compat_dir_create("/SAVES") != 0 ||
        compat_dir_create(SPACE_INVADERS_SAVE_ROOT) != 0) return false;
    compat_file_t *file = compat_file_open(path, "wb");
    if (file == nullptr) return false;
    const bool saved = compat_file_write(g_state_data, 1u, written, file) ==
                       written;
    if (saved)
        std::printf("SPACEINVADERS STATE: saved path=%s bytes=%u\r\n",
                    path, static_cast<unsigned>(written));
    else
        std::printf("SPACEINVADERS STATE: save failed path=%s\r\n", path);
    compat_file_close(file);
    return saved;
}

bool load_state()
{
    char path[96];
    if (!space_invaders_state_path(path, sizeof(path))) return false;
    compat_file_t *file = compat_file_open(path, "rb");
    if (file == nullptr) return false;
    const long size = compat_file_size(file);
    const bool loaded = size > 0 &&
        static_cast<size_t>(size) <= sizeof(g_state_data) &&
        compat_file_read(g_state_data, 1u, static_cast<size_t>(size), file) ==
            static_cast<size_t>(size) &&
        space_invaders_state_load(&g_frontend.board, g_state_data,
                                  static_cast<size_t>(size)) ==
            SPACE_INVADERS_STATE_OK;
    if (loaded)
        std::printf("SPACEINVADERS STATE: loaded path=%s bytes=%ld\r\n",
                    path, size);
    else
        std::printf("SPACEINVADERS STATE: ignored invalid state path=%s\r\n",
                    path);
    compat_file_close(file);
    return loaded;
}

bool start_audio()
{
    compat_audio_ring_init(&g_audio_ring, g_audio_ring_storage,
                           kAudioRingSamples);
    std::memset(g_audio_frame, 0, sizeof(g_audio_frame));
    while (compat_audio_ring_available(&g_audio_ring) < kAudioPrefillSamples)
        (void)compat_audio_ring_write(&g_audio_ring, g_audio_frame,
                                      kAudioFrameMax);
    return compat_audio_start(SPACE_INVADERS_AUDIO_RATE, 1u,
                              audio_callback, nullptr) == 0;
}

retropicoes_menu_result_t shell_start(void *, size_t source_index,
                                     retropicoes_rom_item_kind_t kind)
{
    char error[128] = {};
    if (source_index != 0u || kind != RETROPICOES_ROM_ITEM_ROM ||
        !load_roms(error, sizeof(error))) {
        draw_message("ROM LOAD ERROR", error[0] ? error : "NO GAME ROM",
                     "CHECK /ROMS/SPACEINVADERS");
        std::printf("SPACEINVADERS ROM ERROR: %s\r\n", error);
        return RETROPICOES_MENU_RESULT_FAILED;
    }
    if (!start_audio()) {
        draw_message("AUDIO ERROR", "HDMI AUDIO START FAILED", "PRESS RESET");
        return RETROPICOES_MENU_RESULT_FAILED;
    }
    g_game_active = true;
    std::printf("SPACEINVADERS STATE: cold boot; use Settings > Load state "
                "to restore\r\n");
    std::printf("SPACEINVADERS CONTROLS: Xbox Back=coin Start=play "
                "A/B=fire Guide=Settings\r\n");
    std::printf("SPACEINVADERS READY: rom=%s save=%s video=210x240 "
                "target=60Hz audio=22050Hz\r\n",
                SPACE_INVADERS_ROM_ROOT, SPACE_INVADERS_SAVE_ROOT);
    return RETROPICOES_MENU_RESULT_OK;
}

retropicoes_menu_result_t shell_pause(void *)
{
    std::printf("SPACEINVADERS MENU: paused\r\n");
    compat_audio_stop();
    return RETROPICOES_MENU_RESULT_OK;
}

retropicoes_menu_result_t shell_resume(void *)
{
    std::printf("SPACEINVADERS MENU: resumed\r\n");
    return start_audio() ? RETROPICOES_MENU_RESULT_OK
                         : RETROPICOES_MENU_RESULT_FAILED;
}

retropicoes_menu_result_t shell_reset(void *)
{
    space_invaders_rom_set_t roms = {};
    for (unsigned i = 0u; i < SPACE_INVADERS_ROM_COUNT; ++i) {
        roms.data[i] = g_rom_data[i];
        roms.size[i] = sizeof(g_rom_data[i]);
    }
    return space_invaders_frontend_load_roms(&g_frontend, &roms).status ==
                   SPACE_INVADERS_ROM_OK
               ? RETROPICOES_MENU_RESULT_OK
               : RETROPICOES_MENU_RESULT_FAILED;
}

retropicoes_menu_result_t shell_stop(void *)
{
    if (!g_skip_state_save) save_state();
    std::printf("SPACEINVADERS MENU: stopped save=%u\r\n",
                g_skip_state_save ? 0u : 1u);
    compat_audio_stop();
    g_game_active = false;
    return RETROPICOES_MENU_RESULT_OK;
}

retropicoes_menu_result_t shell_loader(void *)
{
    retropicoes_return_to_bootloader();
    return RETROPICOES_MENU_RESULT_OK;
}

size_t browser_count(void *)
{
    char path[96];
    for (unsigned i = 0u; i < SPACE_INVADERS_ROM_COUNT; ++i) {
        if (!space_invaders_rom_path(i, path, sizeof(path)) ||
            !compat_path_exists(path)) return 0u;
    }
    return 1u;
}

int browser_item(void *, size_t index, retropicoes_rom_item_view_t *item)
{
    if (index != 0u || item == nullptr) return 0;
    item->label = "Space Invaders (4 ROM chips)";
    item->kind = RETROPICOES_ROM_ITEM_ROM;
    return 1;
}

const char *browser_path(void *) { return SPACE_INVADERS_ROM_ROOT; }

bool init_shell_and_browser()
{
    const retropicoes_frontend_lifecycle_t lifecycle = {
        RETROPICOES_FRONTEND_LIFECYCLE_VERSION,
        sizeof(retropicoes_frontend_lifecycle_t), nullptr,
        shell_start, shell_pause, shell_resume, shell_reset,
        shell_stop, shell_loader
    };
    const retropicoes_rom_source_t source = {
        RETROPICOES_ROM_SOURCE_VERSION, sizeof(retropicoes_rom_source_t),
        nullptr, "SPACE INVADERS", "ADD INVADERS.H G F E",
        "DEMO / TEST GAME (SIMULATED)", browser_count, browser_item,
        browser_path
    };
    return retropicoes_frontend_shell_init(&g_shell, &lifecycle) &&
           retropicoes_rom_browser_open(&g_browser, &source, 10u);
}

retropicoes_ui_actions_t browser_actions(retropicoes_menu_repeat_t *repeat)
{
    retropicoes_ui_actions_t held = 0u;
    retropicoes_ui_actions_t pressed = 0u;
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

void browser_loop()
{
    retropicoes_menu_repeat_t repeat;
    retropicoes_menu_repeat_reset(&repeat);
    (void)retropicoes_rom_browser_refresh(&g_browser);
    while (g_shell.screen == RETROPICOES_FRONTEND_ROM_BROWSER) {
        compat_framebuffer_t *fb = compat_framebuffer();
        retropicoes_menu_surface_t surface = {
            fb->pixels, fb->stride, fb->width, fb->height
        };
        const retropicoes_menu_renderer_t renderer =
            retropicoes_menu_nintendo_renderer();
        (void)retropicoes_rom_browser_render(
            &g_browser, &renderer, &surface,
            retropicoes_menu_controller_labels(RETROPICOES_CONTROLLER_XBOX));
        compat_present();
        compat_poll_input();
        const retropicoes_rom_browser_event_t event =
            retropicoes_rom_browser_update(&g_browser,
                                           browser_actions(&repeat));
        if (event.type == RETROPICOES_ROM_BROWSER_EVENT_OPEN) {
            (void)retropicoes_frontend_shell_launch(
                &g_shell, event.source_index, event.item_kind);
        } else if (event.type == RETROPICOES_ROM_BROWSER_EVENT_BACK) {
            retropicoes_return_to_bootloader();
        } else if (event.type == RETROPICOES_ROM_BROWSER_EVENT_SETTINGS &&
                   retropicoes_frontend_shell_open_settings(&g_shell) ==
                       RETROPICOES_MENU_RESULT_OK) {
            const space_invaders_settings_action_t action =
                space_invaders_settings_run();
            (void)retropicoes_frontend_shell_settings_action(
                &g_shell, action == SPACE_INVADERS_SETTINGS_LOADER
                              ? RETROPICOES_FRONTEND_SETTINGS_RETURN_LOADER
                              : RETROPICOES_FRONTEND_SETTINGS_RESUME);
        }
        compat_delay_ms(16u);
    }
}

void uart_pump()
{
    unsigned budget = 32u;
    while (g_uart_position < g_uart_length && budget-- != 0u &&
           uart_is_writable(uart0))
        uart_putc_raw(uart0, g_uart_buffer[g_uart_position++]);
    if (g_uart_position == g_uart_length)
        g_uart_position = g_uart_length = 0u;
}

uint32_t vram_hash()
{
    const uint8_t *vram = &g_frontend.board.memory[SPACE_INVADERS_VRAM_START];
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < SPACE_INVADERS_VRAM_BYTES; ++i)
        hash = (hash ^ vram[i]) * 16777619u;
    return hash;
}

void wait_until(uint64_t deadline)
{
    for (;;) {
        const uint64_t now = compat_ticks_us();
        if (now >= deadline) return;
        const uint64_t remaining = deadline - now;
        if (remaining > 1500u)
            compat_delay_ms(static_cast<uint32_t>((remaining - 500u) / 1000u));
        uart_pump();
    }
}
} // namespace

extern "C" void retropicoes_before_loader_return(void)
{
    save_state();
}

int main()
{
    char error[128] = {};
    if (compat_init() != 0) {
        std::printf("SPACEINVADERS FATAL: HW16 platform initialization failed\r\n");
        return 1;
    }
    if (!space_invaders_frontend_init(&g_frontend) ||
        !init_shell_and_browser()) return 2;
    (void)compat_dir_create("/SAVES");
    (void)compat_dir_create(SPACE_INVADERS_SAVE_ROOT);
    for (;;) {
    browser_loop();
    if (g_shell.screen != RETROPICOES_FRONTEND_RUNNING) return 0;
    bool faulted = false;
    space_invaders_perf_t perf;
    space_invaders_perf_init(&perf, compat_ticks_us());
    retropicoes_video_stats_t previous_video = {};
    retropicoes_video_get_stats(&previous_video);
    g_diag_previous_vram_hash = vram_hash();
    uint64_t deadline = compat_ticks_us() + kFrameBaseUs;
    uint32_t deadline_remainder = 0u;

    while (g_shell.screen == RETROPICOES_FRONTEND_RUNNING) {
        const uint64_t begin = compat_ticks_us();
        compat_poll_input();
        if (retropicoes_input_menu_pressed() &&
            retropicoes_frontend_shell_open_settings(&g_shell) ==
                RETROPICOES_MENU_RESULT_OK) {
            const space_invaders_settings_action_t action =
                space_invaders_settings_run();
            std::printf("SPACEINVADERS MENU: action=%u\r\n",
                        static_cast<unsigned>(action));
            if (action == SPACE_INVADERS_SETTINGS_SAVE) {
                const bool saved = save_state();
                draw_message("SAVE STATE", saved ? "SAVED TO SD" : "SAVE FAILED",
                             saved ? "RESUMING GAME" : "CHECK SD CARD");
                compat_delay_ms(700u);
            }
            if (action == SPACE_INVADERS_SETTINGS_LOAD) {
                const bool loaded = load_state();
                draw_message("LOAD STATE",
                             loaded ? "RESTORED FROM SD" : "NO VALID SAVE",
                             "RESUMING GAME");
                compat_delay_ms(700u);
            }
            const retropicoes_frontend_settings_action_t next =
                action == SPACE_INVADERS_SETTINGS_RESET
                    ? RETROPICOES_FRONTEND_SETTINGS_RESET
                : action == SPACE_INVADERS_SETTINGS_ROM_LIST
                    ? RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER
                : action == SPACE_INVADERS_SETTINGS_LOADER
                    ? RETROPICOES_FRONTEND_SETTINGS_RETURN_LOADER
                    : RETROPICOES_FRONTEND_SETTINGS_RESUME;
            (void)retropicoes_frontend_shell_settings_action(&g_shell, next);
            deadline = compat_ticks_us() + kFrameBaseUs;
            deadline_remainder = 0u;
            retropicoes_video_get_stats(&previous_video);
            space_invaders_perf_init(&perf, compat_ticks_us());
            g_diag_previous_vram_hash = vram_hash();
            continue;
        }
        retropicoes_input_stats_t input = {};
        retropicoes_input_get_stats(&input);
        g_diag_keys_seen |= input.current_mask;
        const uint64_t step_begin = compat_ticks_us();

        g_audio_fraction += SPACE_INVADERS_AUDIO_RATE;
        size_t audio_count = g_audio_fraction / 60u;
        g_audio_fraction %= 60u;
        if (audio_count > kAudioFrameMax) audio_count = kAudioFrameMax;
        const space_invaders_frontend_result_t result =
            space_invaders_frontend_frame(
                &g_frontend,
                input.source_masks[0] | input.source_masks[2],
                input.source_masks[1] | input.source_masks[3],
                compat_ticks_ms(), nullptr, 0u, g_audio_frame, audio_count);
        const uint64_t step_end = compat_ticks_us();
        g_diag_board_inputs_seen |= g_frontend.board.input1;
        g_diag_instructions += result.cpu_instructions;
        if (result.status != SPACE_INVADERS_RUN_OK) {
            std::snprintf(error, sizeof(error), "CORE FAULT %u",
                          static_cast<unsigned>(result.status));
            draw_message("RUNTIME ERROR", error, "PRESS RESET");
            std::printf("SPACEINVADERS RUNTIME ERROR: %u\r\n",
                        static_cast<unsigned>(result.status));
            faulted = true;
            break;
        }

        compat_framebuffer_t *fb = compat_framebuffer();
        if (fb == nullptr || fb->pixels == nullptr ||
            !space_invaders_video_compose(
                &g_frontend.board.memory[SPACE_INVADERS_VRAM_START],
                SPACE_INVADERS_VRAM_BYTES, fb->pixels, fb->stride,
                g_frontend.video_mode)) {
            draw_message("VIDEO ERROR", "FRAME COMPOSE FAILED", "PRESS RESET");
            faulted = true;
            break;
        }
        const uint64_t draw_end = compat_ticks_us();
        compat_present();
        const uint64_t present_end = compat_ticks_us();
        const size_t written = compat_audio_ring_write(
            &g_audio_ring, g_audio_frame, audio_count);
        g_audio_dropped += static_cast<uint32_t>(audio_count - written);
        const uint64_t audio_end = compat_ticks_us();

        retropicoes_video_stats_t video = {};
        retropicoes_audio_stats_t audio = {};
        retropicoes_video_get_stats(&video);
        retropicoes_audio_get_stats(&audio);
        space_invaders_perf_frame_t frame = {};
        frame.input_us = static_cast<uint32_t>(step_begin - begin);
        frame.step_us = static_cast<uint32_t>(step_end - step_begin);
        frame.draw_us = static_cast<uint32_t>(draw_end - step_end);
        frame.present_us = static_cast<uint32_t>(present_end - draw_end);
        frame.audio_us = static_cast<uint32_t>(audio_end - present_end);
        frame.total_us = static_cast<uint32_t>(audio_end - begin);
        frame.displayed_frames = video.displayed_frames -
                                 previous_video.displayed_frames;
        frame.refresh_frames = video.hstx_frames - previous_video.hstx_frames;
        frame.audio_underflow = __atomic_load_n(&g_audio_underflow,
                                                __ATOMIC_RELAXED);
        frame.audio_dropped = g_audio_dropped;
        frame.video_dropped = video.dropped_frames;
        frame.resync_count = video.resync_count;
        previous_video = video;
        space_invaders_perf_add(&perf, &frame);
        if (g_uart_length == 0u) {
            g_uart_length = space_invaders_perf_format(
                &perf, &frame, compat_ticks_us(), g_uart_buffer,
                sizeof(g_uart_buffer));
            g_uart_position = 0u;
            if (g_uart_length != 0u) {
                const uint32_t hash = vram_hash();
                const int added = std::snprintf(
                    g_uart_buffer + g_uart_length,
                    sizeof(g_uart_buffer) - g_uart_length,
                    "SPACEINVADERS DIAG keys_seen=%08lx board_in1_seen=%02x "
                    "instructions=%llu pc=%04x halt=%u vram_changed=%u\r\n",
                    static_cast<unsigned long>(g_diag_keys_seen),
                    g_diag_board_inputs_seen,
                    static_cast<unsigned long long>(g_diag_instructions),
                    g_frontend.board.cpu.cpu.pc,
                    g_frontend.board.cpu.cpu.halted ? 1u : 0u,
                    hash != g_diag_previous_vram_hash ? 1u : 0u);
                if (added > 0 && static_cast<size_t>(added) <
                    sizeof(g_uart_buffer) - g_uart_length)
                    g_uart_length += static_cast<size_t>(added);
                g_diag_keys_seen = 0u;
                g_diag_board_inputs_seen = 0u;
                g_diag_instructions = 0u;
                g_diag_previous_vram_hash = hash;
            }
        } else if (compat_ticks_us() - perf.window_start_us >= 1000000u) {
            space_invaders_perf_note_uart_drop(&perf);
        }
        uart_pump();

        wait_until(deadline);
        deadline += kFrameBaseUs;
        deadline_remainder += kFrameRemainderUs;
        if (deadline_remainder >= 60u) {
            ++deadline;
            deadline_remainder -= 60u;
        }
        const uint64_t now = compat_ticks_us();
        if (now > deadline + 2u * 16667u) deadline = now + kFrameBaseUs;
    }
    if (g_shell.screen == RETROPICOES_FRONTEND_RUNNING) {
        g_skip_state_save = faulted;
        if (faulted) compat_delay_ms(1500u);
        (void)retropicoes_frontend_shell_game_finished(&g_shell);
        g_skip_state_save = false;
    }
    }
}
