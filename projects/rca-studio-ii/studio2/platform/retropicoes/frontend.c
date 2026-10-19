/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "frontend.h"
#include "audio.h"
#include "compat.h"
#include "input_backend.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
static studio2_t machine;
static retropicoes_studio2_frontend_config_t config = {1, 0, 0, 0};
static retropicoes_studio2_frontend_stats_t stats;
static compat_audio_ring_t ring;
static int16_t storage[4096];
static uint32_t under;
static int active, paused;
static uint8_t image[4096];
static retropicoes_studio2_input_debug_t input_debug;
static int tennis_cart;
static int tennis_auto_controls;
static unsigned tennis_setup_frames;
static uint32_t cart_crc(const uint8_t *bytes, size_t count) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < count; i++) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
static void error_text(char *p, unsigned n, const char *s) {
    if (p && n)
        snprintf(p, n, "%s", s);
}
static int read_file(const char *path, size_t *size) {
    compat_file_t *f = compat_file_open(path, "rb");
    long n;
    if (!f)
        return -1;
    n = compat_file_size(f);
    if (n <= 0 || n > (long)sizeof(image)) {
        compat_file_close(f);
        return -2;
    }
    *size = (size_t)n;
    if (compat_file_read(image, 1, *size, f) != *size) {
        compat_file_close(f);
        return -3;
    }
    return compat_file_close(f);
}
static void callback(int16_t *samples, size_t count, void *o) {
    size_t got;
    (void)o;
    got = compat_audio_ring_read(&ring, samples, count);
    __atomic_fetch_add(&under, (uint32_t)(count - got), __ATOMIC_RELAXED);
}
static int start_audio(void) {
    int16_t silence[1024] = {0};
    compat_audio_ring_init(&ring, storage, 4096);
    (void)compat_audio_ring_write(&ring, silence, 1024);
    return compat_audio_start(STUDIO2_AUDIO_RATE, 1, callback, NULL);
}
static int extension_is(const char *extension, const char *expected) {
    if (!extension)
        return 0;
    while (*extension && *expected) {
        if (tolower((unsigned char)*extension++) != (unsigned char)*expected++)
            return 0;
    }
    return *extension == *expected;
}
int retropicoes_studio2_frontend_start(const char *path, int profile, char *error, unsigned cap) {
    size_t size;
    int result;
    const char *ext;
    (void)profile;
    if (active || !path)
        return -1;
    studio2_init(&machine);
    tennis_cart = 0;
    tennis_auto_controls = 0;
    tennis_setup_frames = 0;
    ext = strrchr(path, '.');
    if (extension_is(ext, ".s2d")) {
        if (read_file(path, &size) || studio2_load_bios(&machine, image, size)) {
            error_text(error, cap, "INVALID ORIGINAL DIAGNOSTIC IMAGE");
            return -2;
        }
    } else {
        if (read_file("/bios/studio2.rom", &size) || studio2_load_bios(&machine, image, size)) {
            error_text(error, cap, "NEED 2048 BYTE /bios/studio2.rom");
            return -2;
        }
        if (read_file(path, &size)) {
            error_text(error, cap, "ROM READ OR SIZE ERROR");
            return -3;
        }
        result = studio2_load_cart(&machine, image, size,
                                   extension_is(ext, ".st2") ||
                                       (size >= 4 && !memcmp(image, "RCA2", 4)));
        if (result) {
            error_text(error, cap, "INVALID CART SIZE OR ST2 PAGE MAP");
            return -4;
        }
        tennis_cart = cart_crc(machine.rom + 0x400, 512) == 0xa21590c8u;
        for (unsigned i = 0x600; i < 0x800; i++)
            if (machine.rom[i] != 0xff)
                tennis_cart = 0;
    }
    memset(&stats, 0, sizeof(stats));
    memset(&input_debug, 0, sizeof(input_debug));
    __atomic_store_n(&under, 0, __ATOMIC_RELAXED);
    if (start_audio()) {
        error_text(error, cap, "AUDIO START FAILED");
        return -5;
    }
    active = 1;
    paused = 0;
    if (tennis_cart) {
        tennis_setup_frames = 516;
        tennis_auto_controls = 1;
    }
    return 0;
}
int retropicoes_studio2_frontend_run_frame(void) {
    uint64_t begin, t1, t2, t3, t4, t5;
    unsigned m = 0, k;
    size_t written;
    compat_framebuffer_t *fb;
    retropicoes_input_stats_t input;
    unsigned bank = tennis_auto_controls ? 0 : config.keypad_bank;
    unsigned player = tennis_auto_controls ? 1 : config.keypad_player;
    if (!active || paused)
        return -1;
    begin = compat_ticks_us();
    compat_poll_input();
    if (compat_key_pressed(COMPAT_KEY_BACK))
        return 2;
    retropicoes_input_get_stats(&input);
    for (k = 0; k < COMPAT_KEY_COUNT; k++)
        if (compat_key_down((compat_key_t)k))
            m |= 1u << k;
    machine.keys[0] = machine.keys[1] = 0;
    if ((input.usb_connected_mask & 3u) == 3u) {
        machine.keys[0] = studio2_keypad(input.source_masks[0], bank);
        machine.keys[1] = studio2_keypad(input.source_masks[1], bank);
    } else
        machine.keys[player] = studio2_keypad(m, bank);
    if (tennis_setup_frames) {
        unsigned frame = 516u - tennis_setup_frames;
        machine.keys[0] = machine.keys[1] = 0;
        if (frame >= 120 && frame < 132)
            machine.keys[0] = 1u << 1;
        else if (frame >= 252 && frame < 264)
            machine.keys[1] = 1u << 4;
        else if (frame >= 384 && frame < 396)
            machine.keys[0] = 1u << 7;
        tennis_setup_frames--;
    }
    if (input_debug.raw_mask != m || input_debug.keys[0] != machine.keys[0] ||
        input_debug.keys[1] != machine.keys[1])
        input_debug.events++;
    input_debug.raw_mask = m;
    input_debug.raw_seen |= m;
    memcpy(input_debug.source_masks, input.source_masks, sizeof(input_debug.source_masks));
    input_debug.usb_connected = input.usb_connected_mask;
    for (k = 0; k < 2; k++) {
        input_debug.keys[k] = machine.keys[k];
        input_debug.seen[k] |= machine.keys[k];
    }
    t1 = compat_ticks_us();
    studio2_frame(&machine);
    t2 = compat_ticks_us();
    fb = compat_framebuffer();
    if (!fb || studio2_compose(&machine, fb->pixels, fb->stride, config.scanlines_enabled))
        return -2;
    t3 = compat_ticks_us();
    compat_present();
    t4 = compat_ticks_us();
    if (!config.audio_enabled)
        memset(machine.audio, 0, machine.audio_count * sizeof(machine.audio[0]));
    written = compat_audio_ring_write(&ring, machine.audio, machine.audio_count);
    stats.audio_samples += (uint32_t)written;
    stats.audio_samples_dropped += (uint32_t)(machine.audio_count - written);
    t5 = compat_ticks_us();
    stats.frames++;
    stats.instructions = (uint32_t)machine.cpu.instructions;
    stats.last_input_us = (uint32_t)(t1 - begin);
    stats.last_step_us = (uint32_t)(t2 - t1);
    stats.last_draw_us = (uint32_t)(t3 - t2);
    stats.last_present_us = (uint32_t)(t4 - t3);
    stats.last_audio_us = (uint32_t)(t5 - t4);
    stats.last_frame_us = (uint32_t)(t5 - begin);
    return 0;
}
void retropicoes_studio2_frontend_get_input_debug(retropicoes_studio2_input_debug_t *debug) {
    if (!debug)
        return;
    *debug = input_debug;
    debug->player = tennis_auto_controls ? 1 : config.keypad_player;
    debug->bank = tennis_auto_controls ? 0 : config.keypad_bank;
    debug->latch = machine.key_latch;
    debug->pc = machine.cpu.r[machine.cpu.p];
    debug->frames = machine.frames;
    debug->setup_frames = (uint16_t)tennis_setup_frames;
    memcpy(debug->ef_cycles, machine.keypad_ef_cycles, sizeof(debug->ef_cycles));
    debug->video_hash = 2166136261u;
    for (unsigned i = 0; i < sizeof(machine.video); i++)
        debug->video_hash = (debug->video_hash ^ ((const uint8_t *)machine.video)[i]) * 16777619u;
}
int retropicoes_studio2_frontend_can_start_tennis(void) { return active && tennis_cart; }
int retropicoes_studio2_frontend_start_tennis(void) {
    if (!active || !paused || !tennis_cart)
        return -1;
    studio2_reset(&machine);
    machine.keys[0] = machine.keys[1] = 0;
    memset(&stats, 0, sizeof(stats));
    memset(&input_debug, 0, sizeof(input_debug));
    tennis_setup_frames = 516;
    tennis_auto_controls = 1;
    return 0;
}
int retropicoes_studio2_frontend_pause(void) {
    if (!active)
        return -1;
    compat_audio_stop();
    paused = 1;
    return 0;
}
int retropicoes_studio2_frontend_resume(void) {
    if (!active)
        return -1;
    if (paused) {
        __atomic_store_n(&under, 0, __ATOMIC_RELAXED);
        stats.audio_samples_dropped = 0;
        if (start_audio())
            return -2;
    }
    paused = 0;
    return 0;
}
void retropicoes_studio2_frontend_stop(void) {
    if (active)
        compat_audio_stop();
    active = paused = 0;
}
void retropicoes_studio2_frontend_get_stats(retropicoes_studio2_frontend_stats_t *s) {
    if (s) {
        *s = stats;
        s->audio_samples_underflow = __atomic_load_n(&under, __ATOMIC_RELAXED);
    }
}
void retropicoes_studio2_frontend_set_config(const retropicoes_studio2_frontend_config_t *c) {
    if (c) {
        config = *c;
        tennis_auto_controls = 0;
    }
}
