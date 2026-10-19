/* SPDX-License-Identifier: GPL-3.0-or-later
 * Oric machine wiring, ULA and standard TAP adapter. See doc/SOURCES.md.
 * The chip implementations themselves retain their upstream zlib notices.
 */
#define CHIPS_IMPL
#include "oric.h"
#include <string.h>
#if defined(ORIC_TIMING_PROFILE)
#include "hardware/structs/m33.h"
static inline uint32_t oric_profile_clock(void) { return m33_hw->dwt_cyccnt; }
#endif
#if defined(SDLPoP_PLATFORM_PICO) && defined(__GNUC__)
/* Local placement declarations preserve chip sources and keep the cycle
 * path out of QMI flash contention with the indexed PSRAM video buffer. */
#define ORIC_HOT __attribute__((section(".time_critical.oric")))
uint64_t m6502_tick(m6502_t *, uint64_t) ORIC_HOT;
uint64_t m6522_tick(m6522_t *, uint64_t) ORIC_HOT;
bool ay38910_tick(ay38910_t *) ORIC_HOT;
uint64_t ay38910_iorq(ay38910_t *, uint64_t) ORIC_HOT;
void oric_run_cycles(oric_t *, unsigned) ORIC_HOT;
void oric_render_line(oric_t *, unsigned) ORIC_HOT;
#else
#define ORIC_HOT
#endif

oric_result_t oric_tape_block(const uint8_t *data, size_t size, size_t offset,
                              oric_tape_block_t *out)
{
    if (!data || !out || size > ORIC_MAX_TAPE || offset >= size)
        return ORIC_INVALID_TAPE;
    size_t p = offset;
    while (p < size && data[p] == 0x16) ++p;
    if (p == offset || p >= size || data[p++] != 0x24 || size - p < 9u)
        return ORIC_INVALID_TAPE;
    oric_tape_block_t block = {0};
    block.type = data[p + 2]; block.autorun = data[p + 3];
    block.end = (uint16_t)((unsigned)data[p + 4] * 256u + data[p + 5]);
    block.start = (uint16_t)((unsigned)data[p + 6] * 256u + data[p + 7]);
    p += 9;
    unsigned n = 0;
    while (p < size && data[p]) {
        if (n == 16u) return ORIC_INVALID_TAPE;
        block.name[n++] = (char)data[p++];
    }
    if (p == size || block.end < block.start) return ORIC_INVALID_TAPE;
    ++p;
    const size_t count = (size_t)block.end - block.start + 1u;
    if (size - p < count) return ORIC_INVALID_TAPE;
    if (block.type != 0 && block.type != 0x80) return ORIC_UNSUPPORTED_TAPE;
    block.data_offset = p; block.next_offset = p + count;
    *out = block;
    return ORIC_OK;
}

void oric_rewind_tape(oric_t *s)
{
    s->tape_offset = 0; s->tape_symbol = 0; s->tape_half = 0;
    s->tape_level = 0; s->tape_countdown = 208; s->tape_period = 208;
    s->tape_leader = 256; s->tape_parity = 1;
    s->tape_gap_halves = 0;
    oric_tape_block_t b;
    if (oric_tape_block(s->tape,s->tape_size,0,&b)==ORIC_OK) {
        s->tape_header_end=b.data_offset; s->tape_block_end=b.next_offset;
    }
}

oric_result_t oric_attach_tape(oric_t *s, const uint8_t *data, size_t size)
{
    if (!s) return ORIC_INVALID_ARGUMENT;
    size_t offset = 0;
    do {
        oric_tape_block_t b;
        oric_result_t r = oric_tape_block(data, size, offset, &b);
        if (r != ORIC_OK) return r;
        offset = b.next_offset;
    } while (offset < size);
    s->tape = data; s->tape_size = size;
    oric_rewind_tape(s);
    return ORIC_OK;
}

oric_result_t oric_init(oric_t *s, oric_model_t model, const uint8_t *rom,
                        size_t size, uint8_t *video, size_t video_size)
{
    if (!s || (video ? video_size < ORIC_WIDTH * ORIC_HEIGHT : video_size != 0) ||
        (model != ORIC_MODEL_ORIC1 && model != ORIC_MODEL_ATMOS))
        return ORIC_INVALID_ARGUMENT;
    if (!rom || size != ORIC_ROM_SIZE) return ORIC_INVALID_ROM;
    memset(s, 0, sizeof(*s));
    memcpy(s->memory + 0xc000, rom, ORIC_ROM_SIZE);
    if (video) memset(video, 0, ORIC_WIDTH * ORIC_HEIGHT);
    s->video = video; s->model = model; s->ula_mode = 2;
    s->via_fast_enabled = true;
    s->via_steady_enabled = true;
    s->via_lazy_enabled = true;
    s->via_quiet_shortcut_enabled = true;
    s->ay_read_fast_enabled = true;
    s->ay_tick_fast_enabled = true;
    s->video_cache_enabled = true;
    s->video_dirty = true;
    s->video_full_dirty = true;
    const m6502_desc_t cpu_desc = {0};
    s->pins = m6502_init(&s->cpu, &cpu_desc);
    m6522_init(&s->via);
    const ay38910_desc_t ay_desc = {
        .type = AY38910_TYPE_8912, .tick_hz = ORIC_CLOCK,
        /* Exact integer AY sample period, followed by rational resampling.
         * Upstream's 4-bit fractional period would drift at 22050 Hz. */
        .sound_hz = 25000, .magnitude = 0.25f
    };
    ay38910_init(&s->ay, &ay_desc);
    oric_rewind_tape(s);
    return ORIC_OK;
}

void oric_key(oric_t *s, unsigned key, bool down)
{
    if (!s || key >= 64u) return;
    const uint8_t mask = (uint8_t)(1u << (key & 7u));
    if (down) s->keys[key >> 3] |= mask;
    else s->keys[key >> 3] &= (uint8_t)~mask;
}
void oric_release_keys(oric_t *s) { memset(s->keys, 0, sizeof(s->keys)); }

int oric_ascii_key(char c, bool *shift)
{
    /* Physical matrix, not an imported emulator key table. */
    static const char matrix[64] = {
        '7','N','5','V',0,'1','X','3', 'J','T','R','F',0,27,'Q','D',
        'M','6','B','4',0,'Z','2','C', 'K','9',';','-','#',0,'\\','\'',
        ' ',',','.',0,0,0,0,0, 'U','I','O','P',0,8,']','[',
        'Y','H','G','E',0,'A','S','W', '8','L','0','/',0,13,'`','='
    };
    if (shift) *shift = false;
    if (c == '"') { if (shift) *shift = true; c = '\''; }
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (!c) return -1;
    for (unsigned i = 0; i < 64u; ++i) if (matrix[i] == c) return (int)i;
    return -1;
}

void oric_render_line(oric_t *s, unsigned y)
{
    if (y >= ORIC_HEIGHT) return;
    uint8_t packed_line[ORIC_WIDTH * 3u / 8u];
    uint8_t *packed = s->packed_video ? packed_line : NULL;
    uint8_t *output = packed ? NULL : (s->line_sink ? s->line_pixels :
        (s->video ? s->video + y * ORIC_WIDTH : s->line_pixels));
    uint32_t cells[4];
    unsigned ink = 7, paper = 0, attributes = 0;
    for (unsigned column = 0; column < 40u; ++column) {
        const bool hires = (s->ula_mode & 4u) && y < 200u;
        const unsigned address = hires ? 0xa000u + y * 40u + column :
                                         0xbb80u + (y / 8u) * 40u + column;
        const unsigned cell = s->memory[address];
        unsigned pixels = 0;
        if ((cell & 0x60u) == 0) {
            switch (cell & 0x18u) {
            case 0: ink = cell & 7u; break;
            case 8: attributes = cell & 7u; break;
            case 16: paper = cell & 7u; break;
            case 24: s->ula_mode = cell & 7u; break;
            }
        } else if (hires) {
            pixels = cell & 63u;
        } else {
            const unsigned charset = (s->ula_mode & 4u) ? 0x9800u : 0xb400u;
            const unsigned glyph_y = (attributes & 2u) ? (y / 2u) & 7u : y & 7u;
            pixels = s->memory[charset + ((attributes & 1u) ? 1024u : 0u) +
                               (cell & 127u) * 8u + glyph_y] & 63u;
        }
        if ((attributes & 4u) && !(s->frames & 16u)) pixels = 0;
        if (packed) {
            uint32_t mask = (((pixels & 32u) >> 5u) | ((pixels & 16u) >> 1u) |
                             ((pixels & 8u) << 3u) | ((pixels & 4u) << 7u) |
                             ((pixels & 2u) << 11u) | ((pixels & 1u) << 15u)) * 7u;
            const uint32_t ink18 = ink * 0x9249u;
            const uint32_t paper18 = paper * 0x9249u;
            uint32_t colors = paper18 ^ (mask & (ink18 ^ paper18));
            if (cell & 128u) colors ^= 0x3ffffu;
            cells[column & 3u] = colors;
            if ((column & 3u) == 3u) {
                uint8_t *out = packed + (column >> 2u) * 9u;
                const uint32_t lo = cells[0] | (cells[1] << 18u);
                const uint32_t hi = (cells[1] >> 14u) | (cells[2] << 4u) |
                                    (cells[3] << 22u);
                memcpy(out, &lo, 4u);
                memcpy(out + 4u, &hi, 4u);
                out[8] = (uint8_t)(cells[3] >> 10u);
            }
        } else {
            for (unsigned bit = 0; bit < 6u; ++bit) {
                unsigned color = (pixels & (32u >> bit)) ? ink : paper;
                if (cell & 128u) color ^= 7u;
                output[column * 6u + bit] = (uint8_t)color;
            }
        }
    }
    if (packed) memcpy(s->packed_video + y * sizeof(packed_line), packed_line, sizeof(packed_line));
    else if (s->line_sink) s->line_sink(s->line_context, y, output);
}

void oric_set_line_sink(oric_t *s, oric_line_sink_t sink, void *context)
{
    s->packed_video = NULL;
    s->line_sink = sink; s->line_context = context;
    s->video_dirty = true;
    s->video_full_dirty = true;
}
void oric_set_packed_video(oric_t *s, uint8_t *packed_video)
{
    s->packed_video = packed_video;
    s->line_sink = NULL;
    s->line_context = NULL;
    s->video_dirty = true;
    s->video_full_dirty = true;
}

static ORIC_HOT void tape_tick(oric_t *s)
{
    if (!s->tape || s->tape_offset >= s->tape_size ||
        !(s->via.pb.outr & s->via.pb.ddr & 0x40u)) return;
    if (--s->tape_countdown) return;
    s->tape_level ^= 1u;
    /* Standard TAP omits the short physical gap after the filename.
     * Keep stop pulses running while the ROM prints its loading status. */
    if (s->tape_gap_halves) {
        --s->tape_gap_halves; s->tape_countdown=208; return;
    }
    if (!s->tape_half) {
        const uint8_t data = s->tape_leader ? 0x16u : s->tape[s->tape_offset];
        unsigned bit = 1;
        if (s->tape_symbol == 1) { bit = 0; s->tape_parity = 1; }
        else if (s->tape_symbol >= 2 && s->tape_symbol <= 9) {
            bit = (data >> (s->tape_symbol - 2u)) & 1u;
            s->tape_parity ^= (uint8_t)bit;
        } else if (s->tape_symbol == 10) bit = s->tape_parity;
        s->tape_period = bit ? 208u : 416u;
    }
    s->tape_countdown = s->tape_period;
    s->tape_half ^= 1u;
    if (!s->tape_half && ++s->tape_symbol == 14u) {
        s->tape_symbol = 0;
        if (s->tape_leader) --s->tape_leader;
        else {
            ++s->tape_offset;
            if(s->tape_offset==s->tape_header_end) s->tape_gap_halves=8;
            if(s->tape_offset==s->tape_block_end && s->tape_offset<s->tape_size) {
                oric_tape_block_t b;
                if(oric_tape_block(s->tape,s->tape_size,s->tape_offset,&b)==ORIC_OK) {
                    s->tape_header_end=b.data_offset; s->tape_block_end=b.next_offset;
                    s->tape_leader=256;
                }
            }
        }
    }
}

/* The common Oric cycle has no VIA register access and unchanged external
 * pins. The generic m6522_tick still handles all transitions and all I/O.
 * Stable cycles must advance both timers and the IRQ pipeline exactly once. */
static ORIC_HOT void oric_flush_via(oric_t *s)
{
    s->via_lazy_budget = 0;
    if (!s->via_deferred) return;
    s->via.t1.counter = (uint16_t)(s->via.t1.counter - s->via_deferred);
    s->via.t2.counter = (uint16_t)(s->via.t2.counter - s->via_deferred);
    s->via.pins = s->via_deferred_pins;
    s->via_deferred = 0;
}

static ORIC_HOT uint64_t oric_via_tick(oric_t *s, uint64_t pins, bool io, bool sample)
{
#if !defined(ORIC_TIMING_PROFILE)
    (void)sample;
#endif
    m6522_t *c = &s->via;
    const uint64_t input_mask = M6522_PA_PINS | M6522_PB_PINS |
                                M6522_CA_PINS | M6522_CB_PINS;
    const uint64_t input = pins & input_mask;
    if (s->via_deferred && !s->via_lazy_budget) oric_flush_via(s);
    if (s->via_lazy_enabled && !io && input == s->via_last_input &&
        (s->via_lazy_budget ||
         (s->via_fast_enabled && s->via_steady_enabled && s->via_input_valid &&
          !(c->acr & 0xa0u) && c->t1.pip == 3u && c->t2.pip == 3u &&
          c->intr.pip == 0u && !(c->intr.ifr & c->intr.ier) &&
          !(c->intr.ifr & M6522_IRQ_ANY) &&
          c->t1.counter > 1u && c->t2.counter > 1u))) {
#if defined(ORIC_TIMING_PROFILE)
        if (sample) {
            ++s->profile_via_fast;
            ++s->profile_via_steady;
            ++s->profile_via_lazy;
        }
#endif
        if (!s->via_deferred) {
            s->via_lazy_budget = (uint16_t)((c->t1.counter < c->t2.counter ?
                c->t1.counter : c->t2.counter) - 1u);
            c->pa.c1_triggered = c->pa.c2_triggered = false;
            c->pb.c1_triggered = c->pb.c2_triggered = false;
            c->t1.t_out = c->t2.t_out = false;
        }
        --s->via_lazy_budget;
        uint64_t out = (pins & ~input_mask) | (c->pins & input_mask);
        out &= ~M6522_IRQ;
        s->via_deferred_pins = out;
        ++s->via_deferred;
        return out;
    }
    oric_flush_via(s);
    if (s->via_fast_enabled && s->via_input_valid && !io &&
        input == s->via_last_input && !M6522_ACR_T1_SET_PB7(c)) {
#if defined(ORIC_TIMING_PROFILE)
        if (sample) ++s->profile_via_fast;
#endif
        c->pa.c1_triggered = c->pa.c2_triggered = false;
        c->pb.c1_triggered = c->pb.c2_triggered = false;
        if (s->via_steady_enabled && !(c->acr & 0x20u) &&
            c->t1.pip == 3u && c->t2.pip == 3u &&
            c->t1.counter > 1u && c->t2.counter > 1u) {
#if defined(ORIC_TIMING_PROFILE)
            if (sample) ++s->profile_via_steady;
#endif
            --c->t1.counter;
            --c->t2.counter;
            c->t1.t_out = c->t2.t_out = false;
            if (c->intr.pip & 1u) c->intr.ifr |= M6522_IRQ_ANY;
            pins = (pins & ~input_mask) | (c->pins & input_mask);
            if (c->intr.ifr & M6522_IRQ_ANY) pins |= M6522_IRQ;
            else pins &= ~M6522_IRQ;
            c->intr.pip = (uint16_t)(((c->intr.pip |
                ((c->intr.ifr & c->intr.ier) ? 2u : 0u)) >> 1) & 0x7f7fu);
            c->pins = pins;
            return pins;
        }
        _m6522_tick_t1(c);
        _m6522_tick_t2(c, pins);
        pins = (pins & ~input_mask) | (c->pins & input_mask);
        pins = _m6522_update_irq(c, pins);
        _m6522_tick_pipeline(c);
        c->pins = pins;
        return pins;
    }
    s->via_last_input = input;
    s->via_input_valid = true;
    return m6522_tick(c, pins);
}

static ORIC_HOT void tick(oric_t *s)
{
#if defined(ORIC_TIMING_PROFILE)
    /* Rotate the sampled CPU cycle across raster lines. Sampling only the
     * first cycle of every line biases the per-stage estimate. */
    const bool sample = (s->ticks & 63u) == ((s->ticks >> 6u) & 63u);
    uint32_t stamp = sample ? oric_profile_clock() : 0u;
#endif
    uint64_t pins = m6502_tick(&s->cpu, s->pins);
#if defined(ORIC_TIMING_PROFILE)
    if (sample) { const uint32_t next=oric_profile_clock();s->profile_cycles[0]+=(uint32_t)(next-stamp);stamp=next; }
#endif
    const uint16_t address = M6502_GET_ADDR(pins);
    const bool io = (address & 0xff00u) == 0x0300u;
    uint64_t vp = pins & (0x00ffffffULL | M6522_RW);
    if (io) vp |= M6522_CS1;
    const bool quiet = s->via_quiet_shortcut_enabled && s->via_lazy_budget && !io &&
        (!!(s->via_last_input & M6522_CB1) == !!s->tape_level);
    if (quiet) {
        const uint64_t input_mask = M6522_PA_PINS | M6522_PB_PINS |
                                    M6522_CA_PINS | M6522_CB_PINS;
        vp = (vp & ~input_mask) | (s->via_deferred_pins & input_mask);
        vp &= ~M6522_IRQ;
        s->via_deferred_pins = vp;
        ++s->via_deferred;
        --s->via_lazy_budget;
#if defined(ORIC_TIMING_PROFILE)
        if (sample) {
            ++s->profile_via_fast;
            ++s->profile_via_steady;
            ++s->profile_via_lazy;
        }
#endif
    } else {
    /* Unconnected inputs are pulled high; PB3 is the keyboard sense line. */
    unsigned columns = (s->ay.enable & 0x40u) ? s->ay.port_a : 255u;
    unsigned row = s->via.pb.pins & 7u;
    unsigned pb = 0xf7u;
    if (s->keys[row] & (uint8_t)~columns) pb |= 8u;
    unsigned pa = 255u;
    if (s->ay_control == 1u) {
        if (!s->ay_read_fast_enabled || !s->ay_read_valid || s->ay.in_cb) {
            uint64_t ap = ay38910_iorq(&s->ay, AY38910_BC1);
            s->ay_read_data = AY38910_GET_DATA(ap);
            s->ay_read_valid = !s->ay.in_cb;
        }
        pa = s->ay_read_data;
    } else s->ay_read_valid = false;
    M6522_SET_PA(vp, pa); M6522_SET_PB(vp, pb);
    vp |= M6522_CA1;
    if (s->tape_level) vp |= M6522_CB1;
#if defined(ORIC_TIMING_PROFILE)
    if (sample) { const uint32_t next=oric_profile_clock();s->profile_cycles[5]+=(uint32_t)(next-stamp);stamp=next; }
#endif
    vp = oric_via_tick(s, vp, io,
#if defined(ORIC_TIMING_PROFILE)
                       sample
#else
                       false
#endif
                       );
    }
#if defined(ORIC_TIMING_PROFILE)
    if (sample) { const uint32_t next=oric_profile_clock();s->profile_cycles[1]+=(uint32_t)(next-stamp);stamp=next; }
#endif
    pins = (pins & ~M6502_IRQ) | (vp & M6522_IRQ);
    if (io) {
        if (pins & M6502_RW) M6502_SET_DATA(pins, M6522_GET_DATA(vp));
    } else if (pins & M6502_RW) {
        M6502_SET_DATA(pins, s->memory[address]);
    } else if (address < 0xc000u) {
        const uint8_t data = M6502_GET_DATA(pins);
        if (address >= 0x9800u && s->memory[address] != data) {
            s->video_dirty = true;
            ++s->video_write_generation;
            if (address < 0xa000u || (address >= 0xb400u && address < 0xbc00u))
                s->video_full_dirty = true;
            else {
                const unsigned hires_line = (address - 0xa000u) / 40u;
                if (hires_line < 200u)
                    s->video_dirty_lines[hires_line >> 5] |= 1u << (hires_line & 31u);
                if (address >= 0xbb80u && address < 0xbfe0u) {
                    const unsigned text_row = (address - 0xbb80u) / 40u;
                    s->video_dirty_lines[text_row >> 2] |= 0xffu << ((text_row & 3u) * 8u);
                }
                const uint8_t old = s->memory[address];
                if (((old & 0x78u) == 0x18u) || ((data & 0x78u) == 0x18u))
                    s->video_full_dirty = true;
            }
        }
        s->memory[address] = data;
    }
    unsigned control = ((vp & M6522_CA2) ? 1u : 0u) |
                       ((vp & M6522_CB2) ? 2u : 0u);
    /* A level write every tick would continuously restart AY envelopes. */
    if (control != s->ay_control ||
        (io && !(pins & M6502_RW) && ((address & 15u) == 1u ||
                                                    (address & 15u) == 15u))) {
        uint64_t ap = 0;
        if (control & 1u) ap |= AY38910_BC1;
        if (control & 2u) ap |= AY38910_BDIR;
        AY38910_SET_DATA(ap, M6522_GET_PA(vp));
        if (control) (void)ay38910_iorq(&s->ay, ap);
    }
    s->ay_control = (uint8_t)control;
    tape_tick(s);
#if defined(ORIC_TIMING_PROFILE)
    if (sample) { const uint32_t next=oric_profile_clock();s->profile_cycles[2]+=(uint32_t)(next-stamp);stamp=next; }
#endif
    bool sampled = false;
    if (s->ay_tick_fast_enabled && (s->ay.tick & 7u) != 7u &&
        s->ay.sample_counter > AY38910_FIXEDPOINT_SCALE) {
        ++s->ay.tick;
        s->ay.sample_counter -= AY38910_FIXEDPOINT_SCALE;
    } else sampled = ay38910_tick(&s->ay);
    if (sampled) {
        s->sample_phase += ORIC_AUDIO_RATE;
        if (s->sample_phase >= 25000u) {
            s->sample_phase -= 25000u;
            if (s->audio_count < ORIC_AUDIO_CAPACITY)
                s->audio[s->audio_count++] = (int16_t)(s->ay.sample * 32767.0f);
            else ++s->audio_overflow;
        }
    }
#if defined(ORIC_TIMING_PROFILE)
    if (sample) { const uint32_t next=oric_profile_clock();s->profile_cycles[3]+=(uint32_t)(next-stamp);++s->profile_samples; }
#endif
    s->pins = pins;
    ++s->ticks;
}

void oric_run_cycles(oric_t *s, unsigned cycles)
{
    while (cycles--) tick(s);
    oric_flush_via(s);
}
void oric_run_frame(oric_t *s)
{
    const unsigned lines = (s->ula_mode & 2u) ? 312u : 264u;
    const unsigned top = (lines - ORIC_HEIGHT) / 2u;
    const uint32_t start_video_generation = s->video_write_generation;
    const uint8_t blink = (uint8_t)(s->frames & 16u);
    const bool force_render = !s->video_cache_enabled || s->video_full_dirty ||
                              blink != s->last_render_blink;
    s->video_full_dirty = false;
    s->video_changed = false;
    s->audio_count = 0;
    for (unsigned line = 0; line < lines; ++line) {
        oric_run_cycles(s, 64);
        if (line >= top && line < top + ORIC_HEIGHT) {
            const unsigned y = line - top;
            const uint32_t mask = 1u << (y & 31u);
            if (force_render || s->video_full_dirty || (s->video_dirty_lines[y >> 5] & mask)) {
#if defined(ORIC_TIMING_PROFILE)
            const uint32_t stamp=oric_profile_clock();
#endif
            oric_render_line(s, y);
            s->video_changed = true;
            s->line_mode_after[y] = s->ula_mode;
            s->video_dirty_lines[y >> 5] &= ~mask;
#if defined(ORIC_TIMING_PROFILE)
            s->profile_cycles[4]+=(uint32_t)(oric_profile_clock()-stamp);
            ++s->profile_lines;
#endif
            } else s->ula_mode = s->line_mode_after[y];
        }
    }
    s->video_dirty = s->video_write_generation != start_video_generation;
    s->last_render_blink = blink;
    s->frame_cycles = lines * 64u;
    ++s->frames;
}
void oric_jump(oric_t *s, uint16_t address)
{
    m6502_set_pc(&s->cpu, address);
    s->pins = M6502_SYNC | M6502_RW;
    M6502_SET_ADDR(s->pins, address);
    M6502_SET_DATA(s->pins, s->memory[address]);
}
oric_result_t oric_load_binary(oric_t *s, const uint8_t *data, size_t size)
{
    oric_tape_block_t b;
    oric_result_t r = oric_tape_block(data, size, 0, &b);
    if (r != ORIC_OK) return r;
    if (!s) return ORIC_INVALID_ARGUMENT;
    if (b.type != 0x80 || b.start < 0x0400 || b.end >= 0xc000 ||
        b.next_offset != size) return ORIC_UNSUPPORTED_TAPE;
    memcpy(s->memory + b.start, data + b.data_offset,
           (size_t)b.end - b.start + 1u);
    s->video_dirty = true;
    s->video_full_dirty = true;
    oric_jump(s, b.start);
    return ORIC_OK;
}
