/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "compat.h"
#include "diagnostic.h"
#include "frontend.h"
#include "input_backend.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct compat_file {
    const unsigned char *data;
    size_t size;
};
static struct compat_file file;
static int no_bios, bad_bios, bad_cart, audio_fail, audio_running, home;
static uint64_t clock_us;
static uint16_t surface[320 * 240];
static compat_framebuffer_t fb = {surface, 320, 320, 240};
static unsigned char cart[1025];
static unsigned input_mask;
static retropicoes_input_stats_t input_stats;
static unsigned char real_bios[2048];
static int real_tennis;
compat_file_t *compat_file_open(const char *path, const char *mode) {
    (void)mode;
    if (!strcmp(path, "/bios/studio2.rom")) {
        if (no_bios)
            return NULL;
        file.data = real_tennis ? real_bios : diagnostic;
        file.size = bad_bios ? 2047 : 2048;
    } else if (strstr(path, ".s2d")) {
        file.data = diagnostic;
        file.size = 2048;
    } else {
        file.data = cart;
        file.size = bad_cart ? 1025 : (real_tennis ? 512 : 1024);
    }
    return &file;
}
long compat_file_size(compat_file_t *f) { return (long)f->size; }
size_t compat_file_read(void *p, size_t size, size_t n, compat_file_t *f) {
    if (size * n > f->size)
        return 0;
    memcpy(p, f->data, size * n);
    return n;
}
int compat_file_close(compat_file_t *f) {
    (void)f;
    return 0;
}
uint64_t compat_ticks_us(void) { return clock_us += 10; }
void compat_poll_input(void) {}
int compat_key_down(compat_key_t k) {
    return (input_mask & (1u << k)) != 0;
}
int compat_key_pressed(compat_key_t k) { return k == COMPAT_KEY_BACK && home; }
void retropicoes_input_get_stats(retropicoes_input_stats_t *s) { *s = input_stats; }
compat_framebuffer_t *compat_framebuffer(void) { return &fb; }
void compat_present(void) {}
int compat_audio_start(unsigned rate, unsigned channels, compat_audio_callback_t cb, void *user) {
    (void)cb;
    (void)user;
    assert(rate == 22050 && channels == 1);
    if (audio_fail)
        return -1;
    audio_running = 1;
    return 0;
}
void compat_audio_stop(void) { audio_running = 0; }
static void read_exact(const char *path, unsigned char *bytes, size_t size) {
    FILE *f = fopen(path, "rb");
    assert(f && fread(bytes, 1, size, f) == size && fgetc(f) == EOF);
    assert(!fclose(f));
}
int main(int argc, char **argv) {
    char error[96];
    retropicoes_studio2_frontend_stats_t s;
    no_bios = 1;
    assert(retropicoes_studio2_frontend_start("/roms/STUDIO2/game.bin", 0, error, sizeof(error)) <
               0 &&
           !audio_running);
    assert(strstr(error, "bios"));
    no_bios = 0;
    bad_bios = 1;
    assert(retropicoes_studio2_frontend_start("/roms/STUDIO2/game.bin", 0, error, sizeof(error)) <
           0);
    bad_bios = 0;
    bad_cart = 1;
    assert(retropicoes_studio2_frontend_start("/roms/STUDIO2/game.bin", 0, error, sizeof(error)) <
           0);
    bad_cart = 0;
    assert(retropicoes_studio2_frontend_start("/roms/STUDIO2/bad.st2", 0, error, sizeof(error)) <
           0);
    audio_fail = 1;
    assert(retropicoes_studio2_frontend_start("/roms/STUDIO2/test.s2d", 0, error, sizeof(error)) <
           0);
    audio_fail = 0;
    no_bios = 1;
    assert(!retropicoes_studio2_frontend_start("/roms/STUDIO2/test.s2d", 0, error, sizeof(error)) &&
           audio_running);
    assert(!retropicoes_studio2_frontend_can_start_tennis());
    assert(retropicoes_studio2_frontend_start_tennis() < 0);
    assert(retropicoes_studio2_frontend_start("test.s2d", 0, error, sizeof(error)) < 0);
    assert(!retropicoes_studio2_frontend_run_frame());
    retropicoes_studio2_frontend_get_stats(&s);
    assert(s.frames == 1 && s.instructions > 0 && s.audio_samples > 0);
    retropicoes_studio2_frontend_config_t config = {1, 0, 0, 1};
    retropicoes_studio2_input_debug_t debug;
    retropicoes_studio2_frontend_set_config(&config);
    input_stats.usb_name[0] = "controller";
    input_stats.usb_name[1] = "disconnected controller";
    input_stats.usb_connected_mask = 1;
    input_mask = input_stats.source_masks[0] = 1u << COMPAT_KEY_START;
    assert(!retropicoes_studio2_frontend_run_frame());
    retropicoes_studio2_frontend_get_input_debug(&debug);
    assert(debug.keys[0] == 0 && debug.keys[1] == 2 && debug.player == 1);
    input_stats.usb_connected_mask = 2;
    input_stats.source_masks[0] = 0;
    input_stats.source_masks[1] = input_mask;
    assert(!retropicoes_studio2_frontend_run_frame());
    retropicoes_studio2_frontend_get_input_debug(&debug);
    assert(debug.keys[0] == 0 && debug.keys[1] == 2);
    input_stats.usb_connected_mask = 3;
    input_stats.source_masks[0] = 1u << COMPAT_KEY_START;
    input_stats.source_masks[1] = 1u << COMPAT_KEY_LEFT;
    input_mask = input_stats.source_masks[0] | input_stats.source_masks[1];
    assert(!retropicoes_studio2_frontend_run_frame());
    retropicoes_studio2_frontend_get_input_debug(&debug);
    assert(debug.keys[0] == 2 && debug.keys[1] == 16);
    input_stats.usb_connected_mask = 0;
    input_stats.source_masks[0] = input_stats.source_masks[1] = 0;
    input_mask = input_stats.source_masks[2] = 1u << COMPAT_KEY_UP;
    assert(!retropicoes_studio2_frontend_run_frame());
    retropicoes_studio2_frontend_get_input_debug(&debug);
    assert(debug.keys[0] == 0 && debug.keys[1] == 4);
    input_mask = input_stats.source_masks[2] = 0;
    assert(!retropicoes_studio2_frontend_run_frame());
    retropicoes_studio2_frontend_get_input_debug(&debug);
    assert(debug.keys[0] == 0 && debug.keys[1] == 0 && debug.events >= 4);
    assert(debug.seen[0] == 2 && debug.seen[1] == (2 | 4 | 16));
    home = 1;
    assert(retropicoes_studio2_frontend_run_frame() == 2);
    retropicoes_studio2_frontend_get_stats(&s);
    assert(s.frames == 6);
    home = 0;
    assert(!retropicoes_studio2_frontend_pause() && !audio_running);
    assert(retropicoes_studio2_frontend_run_frame() < 0);
    assert(!retropicoes_studio2_frontend_resume() && audio_running);
    assert(!retropicoes_studio2_frontend_run_frame());
    retropicoes_studio2_frontend_stop();
    assert(!audio_running && retropicoes_studio2_frontend_run_frame() < 0);
    if (argc == 3) {
        read_exact(argv[1], real_bios, sizeof(real_bios));
        read_exact(argv[2], cart, 512);
        real_tennis = 1;
        no_bios = 0;
        input_mask = 0;
        memset(&input_stats, 0, sizeof(input_stats));
        config.keypad_player = 0;
        config.keypad_bank = 1;
        retropicoes_studio2_frontend_set_config(&config);
        assert(!retropicoes_studio2_frontend_start("tennis.bin", 0, error, sizeof(error)));
        assert(retropicoes_studio2_frontend_can_start_tennis());
        retropicoes_studio2_frontend_get_input_debug(&debug);
        assert(debug.setup_frames == 516 && debug.player == 1 && debug.bank == 0);
        assert(retropicoes_studio2_frontend_start_tennis() < 0);
        assert(!retropicoes_studio2_frontend_pause());
        assert(retropicoes_studio2_frontend_run_frame() < 0);
        retropicoes_studio2_frontend_get_input_debug(&debug);
        assert(debug.setup_frames == 516);
        assert(!retropicoes_studio2_frontend_resume());
        for (unsigned i = 0; i < 516; ++i)
            assert(!retropicoes_studio2_frontend_run_frame());
        retropicoes_studio2_frontend_get_input_debug(&debug);
        assert(debug.setup_frames == 0 && debug.seen[0] == (2 | 128) && debug.seen[1] == 16);
        assert(debug.ef_cycles[0] > 0 && debug.ef_cycles[1] > 0);
        unsigned changes = 0;
        uint32_t previous = debug.video_hash;
        for (unsigned i = 0; i < 600; ++i) {
            assert(!retropicoes_studio2_frontend_run_frame());
            retropicoes_studio2_frontend_get_input_debug(&debug);
            changes += previous != debug.video_hash;
            previous = debug.video_hash;
        }
        assert(changes >= 10);
        input_mask = 1u << COMPAT_KEY_UP;
        assert(!retropicoes_studio2_frontend_run_frame());
        retropicoes_studio2_frontend_get_input_debug(&debug);
        assert(debug.keys[0] == 0 && debug.keys[1] == 4);
        assert(!retropicoes_studio2_frontend_pause());
        assert(!retropicoes_studio2_frontend_start_tennis());
        retropicoes_studio2_frontend_get_input_debug(&debug);
        assert(debug.setup_frames == 516 && debug.frames == 0);
        assert(!retropicoes_studio2_frontend_resume());
        assert(!retropicoes_studio2_frontend_run_frame());
        retropicoes_studio2_frontend_stop();
        real_tennis = 0;
        input_mask = 0;
        assert(!retropicoes_studio2_frontend_start("test.s2d", 0, error, sizeof(error)));
        retropicoes_studio2_frontend_get_input_debug(&debug);
        assert(debug.setup_frames == 0 && debug.player == 0 && debug.bank == 1);
        retropicoes_studio2_frontend_stop();
        puts("PRODUCTION FRONTEND TENNIS: direct launch, A1/B4/A7 setup, B2 input, pause, "
             "restart and non-Tennis config restoration PASS");
    }
    puts("STUDIO2 FRONTEND: BIOS/cart/audio failures, independent diagnostics, pause/resume/stop "
         "and Menu handoff PASS");
    return 0;
}
