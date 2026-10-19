#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/sync.h"

#include "be_st.h"
#include "backend/audio/be_audio_mixer.h"
#include "backend/audio/be_audio_private.h"

#include "compat.h"
#include "frontend.h"
#include "hstx_data_island_queue.h"
#include "hstx_packet.h"
#include "native_game_settings.h"
#include "video_output.h"

enum {
    REFKEEN_AUDIO_RATE = 48000u,
    REFKEEN_AUDIO_CHANNELS = 2u,
    REFKEEN_BLOCK_FRAMES = 64u,
    REFKEEN_PACKET_FRAMES = 4u,
    REFKEEN_PACKETS_PER_TASK = 16u,
    REFKEEN_QUEUE_TARGET = 192u
};

static recursive_mutex_t g_audio_mutex;
static bool g_mutex_ready;
static bool g_running;
static bool g_task_active;
static BE_ST_SndSample_T g_samples[REFKEEN_BLOCK_FRAMES * REFKEEN_AUDIO_CHANNELS];
static unsigned g_frame_index = REFKEEN_BLOCK_FRAMES;
static uint32_t g_iec_frame_count;

static bool audio_running(void)
{
    return __atomic_load_n(&g_running, __ATOMIC_ACQUIRE);
}

static void fill_block(void)
{
    recursive_mutex_enter_blocking(&g_audio_mutex);
    if (retropicoes_refkeen_frontend_is_paused() ||
        !retropicoes_native_game_audio_enabled()) {
        memset(g_samples, 0, sizeof(g_samples));
    } else {
        BEL_ST_AudioMixerCallback(g_samples, REFKEEN_BLOCK_FRAMES);
    }
    recursive_mutex_exit(&g_audio_mutex);
    g_frame_index = 0u;
}

static void __not_in_flash_func(retropicoes_refkeen_audio_task)(void)
{
    unsigned budget;

    if (!audio_running())
        return;
    __atomic_store_n(&g_task_active, true, __ATOMIC_RELEASE);

    for (budget = 0u;
         audio_running() && budget < REFKEEN_PACKETS_PER_TASK &&
         hstx_di_queue_get_level() < REFKEEN_QUEUE_TARGET;
         ++budget) {
        audio_sample_t output[REFKEEN_PACKET_FRAMES];
        hstx_packet_t packet;
        hstx_data_island_t island;
        unsigned frame;

        if (g_frame_index + REFKEEN_PACKET_FRAMES > REFKEEN_BLOCK_FRAMES)
            fill_block();

        for (frame = 0u; frame < REFKEEN_PACKET_FRAMES; ++frame) {
            const unsigned index = 2u * (g_frame_index + frame);
            output[frame].left = g_samples[index];
            output[frame].right = g_samples[index + 1u];
        }

        g_iec_frame_count = (uint32_t)hstx_packet_set_audio_samples_cs(
            &packet, output, REFKEEN_PACKET_FRAMES, (int)g_iec_frame_count);
        hstx_encode_data_island(&island, &packet, false, true);
        if (!hstx_di_queue_push(&island))
            break;
        g_frame_index += REFKEEN_PACKET_FRAMES;
    }

    __atomic_store_n(&g_task_active, false, __ATOMIC_RELEASE);
}

bool BEL_ST_InitAudioSubsystem(int *freq, int *channels, int *bufferLen)
{
    if (!freq || !channels || !bufferLen)
        return false;
#if REFKEEN_DIAG_DISABLE_HDMI_AUDIO
    printf("REFKEEN AUDIO DIAG: HDMI audio disabled for video isolation\n");
    return false;
#endif
    if (video_output_get_dvi_mode()) {
        printf("REFKEEN AUDIO: disabled in DVI mode\n");
        return false;
    }

    if (!g_mutex_ready) {
        recursive_mutex_init(&g_audio_mutex);
        g_mutex_ready = true;
    }

    *freq = REFKEEN_AUDIO_RATE;
    *channels = REFKEEN_AUDIO_CHANNELS;
    *bufferLen = REFKEEN_BLOCK_FRAMES;
    memset(g_samples, 0, sizeof(g_samples));
    g_frame_index = REFKEEN_BLOCK_FRAMES;
    g_iec_frame_count = 0u;
    pico_hdmi_set_audio_sample_rate(REFKEEN_AUDIO_RATE);
    printf("REFKEEN AUDIO: mixer=%uHz stereo hdmi=%uHz\n",
           REFKEEN_AUDIO_RATE, REFKEEN_AUDIO_RATE);
    return true;
}

void BEL_ST_StartAudioSubsystem(void)
{
    printf("REFKEEN STAGE: audio start begin\n");
    if (!g_mutex_ready || audio_running())
        return;
    __atomic_store_n(&g_running, true, __ATOMIC_RELEASE);
    video_output_set_background_task(retropicoes_refkeen_audio_task);
    printf("REFKEEN STAGE: audio start complete\n");
}

void BEL_ST_ShutdownAudioSubsystem(void)
{
    if (!g_mutex_ready)
        return;
    __atomic_store_n(&g_running, false, __ATOMIC_RELEASE);
    video_output_set_background_task(NULL);
    while (__atomic_load_n(&g_task_active, __ATOMIC_ACQUIRE))
        tight_loop_contents();
    recursive_mutex_enter_blocking(&g_audio_mutex);
    BEL_ST_AudioMixerShutdown();
    recursive_mutex_exit(&g_audio_mutex);
}

void BE_ST_LockAudioRecursively(void)
{
    if (g_mutex_ready)
        recursive_mutex_enter_blocking(&g_audio_mutex);
}

void BE_ST_UnlockAudioRecursively(void)
{
    if (g_mutex_ready)
        recursive_mutex_exit(&g_audio_mutex);
}

/* compat_shutdown() uses this frozen compat-v1 hook. ReflectionHLE otherwise
 * controls the same backend through BEL_ST_* above. */
int compat_audio_start(unsigned sample_rate, unsigned channels,
                       compat_audio_callback_t callback, void *user_data)
{
    (void)callback;
    (void)user_data;
    if (sample_rate != REFKEEN_AUDIO_RATE ||
        channels != REFKEEN_AUDIO_CHANNELS)
        return -1;
    BEL_ST_StartAudioSubsystem();
    return 0;
}

void compat_audio_stop(void)
{
    if (!g_mutex_ready)
        return;
    __atomic_store_n(&g_running, false, __ATOMIC_RELEASE);
    video_output_set_background_task(NULL);
    while (__atomic_load_n(&g_task_active, __ATOMIC_ACQUIRE))
        tight_loop_contents();
}
