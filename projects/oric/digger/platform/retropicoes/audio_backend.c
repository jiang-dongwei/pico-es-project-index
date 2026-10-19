#include "audio_backend.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "pico.h"
#include "pico/stdlib.h"

#include "compat.h"
#include "hstx_data_island_queue.h"
#include "hstx_packet.h"
#include "video_backend.h"
#include "video_output.h"

enum {
    DIGGER_SAMPLE_RATE = 22050u,
    HDMI_SAMPLE_RATE = 44100u,
    SOURCE_BLOCK_SAMPLES = 64u,
    AUDIO_PACKET_FRAMES = 4u,
    PACKETS_PER_TASK = 16u,
    QUEUE_TARGET_PACKETS = 192u
};

static compat_audio_callback_t g_callback;
static void *g_callback_user_data;
static int16_t g_source_block[SOURCE_BLOCK_SAMPLES];
static uint32_t g_source_index = SOURCE_BLOCK_SAMPLES;
static uint32_t g_iec_frame_count;
static uint32_t g_underrun_baseline;
static uint32_t g_callback_calls;
static uint32_t g_source_samples;
static uint32_t g_output_frames;
static uint32_t g_packets_pushed;
static uint32_t g_queue_full_events;
static uint32_t g_source_sample_rate;
static uint8_t g_source_channels;
static bool g_running;
static bool g_task_active;

static bool load_running(void)
{
    return __atomic_load_n(&g_running, __ATOMIC_ACQUIRE);
}

static void store_running(bool running)
{
    __atomic_store_n(&g_running, running, __ATOMIC_RELEASE);
}

static void add_counter(uint32_t *counter, uint32_t amount)
{
    (void)__atomic_fetch_add(counter, amount, __ATOMIC_RELAXED);
}

static uint32_t load_counter(const uint32_t *counter)
{
    return __atomic_load_n(counter, __ATOMIC_RELAXED);
}

static void __not_in_flash_func(retropicoes_audio_core1_task)(void)
{
    unsigned packet_budget;

    if (!load_running()) return;
    __atomic_store_n(&g_task_active, true, __ATOMIC_RELEASE);
    if (!load_running()) {
        __atomic_store_n(&g_task_active, false, __ATOMIC_RELEASE);
        return;
    }

    for (packet_budget = 0u;
         packet_budget < PACKETS_PER_TASK &&
         hstx_di_queue_get_level() < QUEUE_TARGET_PACKETS;
         ++packet_budget) {
        audio_sample_t output[AUDIO_PACKET_FRAMES];
        hstx_packet_t packet;
        hstx_data_island_t island;
        uint32_t next_frame_count;
        int16_t first;
        int16_t second;

        if (g_source_index + 2u > SOURCE_BLOCK_SAMPLES) {
            compat_audio_callback_t callback = g_callback;
            if (callback == NULL) break;
            callback(g_source_block, SOURCE_BLOCK_SAMPLES,
                     g_callback_user_data);
            g_source_index = 0u;
            add_counter(&g_callback_calls, 1u);
            add_counter(&g_source_samples, SOURCE_BLOCK_SAMPLES);
        }

        first = g_source_block[g_source_index];
        second = g_source_block[g_source_index + 1u];
        output[0].left = first;
        output[0].right = first;
        output[1].left = first;
        output[1].right = first;
        output[2].left = second;
        output[2].right = second;
        output[3].left = second;
        output[3].right = second;

        next_frame_count = (uint32_t)hstx_packet_set_audio_samples_cs(
            &packet, output, AUDIO_PACKET_FRAMES, (int)g_iec_frame_count);
        hstx_encode_data_island(&island, &packet, false, true);
        if (!hstx_di_queue_push(&island)) {
            add_counter(&g_queue_full_events, 1u);
            break;
        }

        g_source_index += 2u;
        g_iec_frame_count = next_frame_count;
        add_counter(&g_output_frames, AUDIO_PACKET_FRAMES);
        add_counter(&g_packets_pushed, 1u);
    }

    __atomic_store_n(&g_task_active, false, __ATOMIC_RELEASE);
}

int compat_audio_start(unsigned sample_rate, unsigned channels,
                       compat_audio_callback_t callback, void *user_data)
{
    if (load_running()) {
        return sample_rate == g_source_sample_rate &&
                       channels == g_source_channels
                   ? 0
                   : -1;
    }
    if (sample_rate != DIGGER_SAMPLE_RATE || channels != 1u ||
        callback == NULL) {
        return -1;
    }
    if (!retropicoes_video_ready() || video_output_get_dvi_mode()) {
        return -2;
    }

    g_callback = callback;
    g_callback_user_data = user_data;
    memset(g_source_block, 0, sizeof(g_source_block));
    g_source_index = SOURCE_BLOCK_SAMPLES;
    g_iec_frame_count = 0u;
    g_callback_calls = 0u;
    g_source_samples = 0u;
    g_output_frames = 0u;
    g_packets_pushed = 0u;
    g_queue_full_events = 0u;
    g_source_sample_rate = sample_rate;
    g_source_channels = (uint8_t)channels;

    pico_hdmi_set_audio_sample_rate(HDMI_SAMPLE_RATE);
    g_underrun_baseline = hstx_di_queue_get_underrun_count();
    store_running(true);
    video_output_set_background_task(retropicoes_audio_core1_task);
    printf("AUDIO: source=%uHz mono hdmi=%uHz stereo core1=HSTX\n",
           sample_rate, (unsigned)HDMI_SAMPLE_RATE);
    return 0;
}

void compat_audio_stop(void)
{
    if (!load_running()) return;
    store_running(false);
    video_output_set_background_task(NULL);
    while (__atomic_load_n(&g_task_active, __ATOMIC_ACQUIRE)) {
        tight_loop_contents();
    }
    g_callback = NULL;
    g_callback_user_data = NULL;
}

int retropicoes_audio_ready(void)
{
    return retropicoes_video_ready() && !video_output_get_dvi_mode() ? 1 : 0;
}

void retropicoes_audio_get_stats(retropicoes_audio_stats_t *stats)
{
    if (stats == NULL) return;
    stats->callback_calls = load_counter(&g_callback_calls);
    stats->source_samples = load_counter(&g_source_samples);
    stats->output_frames = load_counter(&g_output_frames);
    stats->packets_pushed = load_counter(&g_packets_pushed);
    stats->queue_full_events = load_counter(&g_queue_full_events);
    stats->underruns = hstx_di_queue_get_underrun_count() -
                       g_underrun_baseline;
    stats->queue_level = hstx_di_queue_get_level();
    stats->source_sample_rate = g_source_sample_rate;
    stats->hdmi_sample_rate = pico_hdmi_get_audio_sample_rate();
    stats->source_channels = g_source_channels;
    stats->ready = (uint8_t)retropicoes_audio_ready();
    stats->running = (uint8_t)(load_running() ? 1u : 0u);
}
