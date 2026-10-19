/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PICOES_ORIC_H
#define PICOES_ORIC_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "m6502.h"
#include "m6522.h"
#include "ay38910.h"
#ifdef __cplusplus
extern "C" {
#endif

enum { ORIC_WIDTH = 240, ORIC_HEIGHT = 224, ORIC_ROM_SIZE = 16384,
       ORIC_CLOCK = 1000000, ORIC_AUDIO_RATE = 22050,
       ORIC_AUDIO_CAPACITY = 512, ORIC_MAX_TAPE = 1024 * 1024 };
typedef enum { ORIC_MODEL_ORIC1, ORIC_MODEL_ATMOS } oric_model_t;
typedef enum { ORIC_OK, ORIC_INVALID_ROM, ORIC_INVALID_TAPE,
               ORIC_UNSUPPORTED_TAPE, ORIC_INVALID_ARGUMENT } oric_result_t;
/* Matrix positions, matching the physical keyboard row/column wiring. */
enum { ORIC_KEY_SPACE = 32, ORIC_KEY_UP = 35, ORIC_KEY_SHIFT = 36,
       ORIC_KEY_LEFT = 37, ORIC_KEY_DOWN = 38, ORIC_KEY_RIGHT = 39,
       ORIC_KEY_CTRL = 20, ORIC_KEY_RETURN = 61, ORIC_KEY_ESCAPE = 13 };
typedef struct {
    uint16_t start, end;
    uint8_t type, autorun;
    size_t data_offset, next_offset;
    char name[17];
} oric_tape_block_t;
typedef void (*oric_line_sink_t)(void *context, unsigned y, const uint8_t *pixels);

typedef struct {
    m6502_t cpu;
    m6522_t via;
    ay38910_t ay;
    uint64_t pins;
    uint8_t memory[65536];
    uint8_t keys[8];
    uint64_t via_last_input;
    bool via_input_valid, via_fast_enabled, via_steady_enabled;
    bool via_lazy_enabled;
    bool via_quiet_shortcut_enabled;
    uint16_t via_deferred;
    uint16_t via_lazy_budget;
    uint64_t via_deferred_pins;
    uint8_t *video; /* caller-owned 240x224 index buffer */
    uint8_t *packed_video; /* caller-owned 90x224 3-bit index buffer */
    oric_line_sink_t line_sink;
    void *line_context;
    uint8_t line_pixels[ORIC_WIDTH]; /* SRAM staging for direct scanline output */
    uint8_t line_mode_after[ORIC_HEIGHT];
    uint32_t video_dirty_lines[7];
    uint32_t video_write_generation;
    uint8_t last_render_blink;
    bool video_dirty, video_full_dirty, video_cache_enabled, video_changed;
    const uint8_t *tape; /* caller-owned, immutable until detach/reset */
    size_t tape_size, tape_offset;
    size_t tape_header_end, tape_block_end;
    uint32_t frames, ticks;
    unsigned frame_cycles;
    uint8_t ula_mode, ay_control;
    uint8_t ay_read_data;
    bool ay_read_valid, ay_read_fast_enabled, ay_tick_fast_enabled;
    uint8_t tape_level, tape_symbol, tape_half, tape_parity;
    unsigned tape_countdown, tape_period, tape_leader;
    unsigned tape_gap_halves;
    int16_t audio[ORIC_AUDIO_CAPACITY];
    size_t audio_count;
    uint32_t audio_overflow;
    unsigned sample_phase;
    oric_model_t model;
#if defined(ORIC_TIMING_PROFILE)
    uint64_t profile_cycles[6]; /* CPU, VIA, bus/tape, AY, ULA, VIA prep */
    uint32_t profile_samples, profile_lines;
    uint32_t profile_via_fast;
    uint32_t profile_via_steady;
    uint32_t profile_via_lazy;
#endif
} oric_t;

oric_result_t oric_init(oric_t *, oric_model_t, const uint8_t *, size_t,
                        uint8_t *video, size_t video_size);
/* video may be NULL/0 for streaming; install sink before running frames.
 * Sink consumes each line synchronously and must not retain its pointer. */
void oric_set_line_sink(oric_t *, oric_line_sink_t, void *context);
void oric_set_packed_video(oric_t *, uint8_t *packed_video);
oric_result_t oric_tape_block(const uint8_t *, size_t, size_t,
                              oric_tape_block_t *);
oric_result_t oric_attach_tape(oric_t *, const uint8_t *, size_t);
void oric_rewind_tape(oric_t *);
void oric_key(oric_t *, unsigned matrix_key, bool down);
int oric_ascii_key(char ch, bool *shift);
void oric_release_keys(oric_t *);
void oric_run_frame(oric_t *);
void oric_run_cycles(oric_t *, unsigned cycles);
void oric_render_line(oric_t *, unsigned y);
void oric_jump(oric_t *, uint16_t address);
/* Test/direct-load boundary; rejects BASIC and writes outside RAM. */
oric_result_t oric_load_binary(oric_t *, const uint8_t *, size_t);
#ifdef __cplusplus
}
#endif
#endif
