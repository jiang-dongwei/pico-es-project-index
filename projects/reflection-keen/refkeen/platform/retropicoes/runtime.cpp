#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <limits>

extern "C" {
#include "compat.h"
#include "ff.h"
#include "SDL3/SDL_mutex.h"
}

#include "FrensHelpers.h"
#include "hardware/clocks.h"
#include "hardware/psram.h"
#include "hardware/vreg.h"
#include "lwmem/lwmem.h"
#include "pico/mutex.h"
#include "pico/stdlib.h"

#include "runtime.h"

extern "C" int retropicoes_input_init(void);
extern "C" void retropicoes_input_shutdown(void);
extern "C" int retropicoes_video_init(void);
extern "C" void retropicoes_video_shutdown(void);

namespace Frens {
bool initPsram();
void configQspiPads();
int enableFlashQuad6B();
}

namespace {
constexpr uint32_t kSystemClockKHz = 252000u;
constexpr size_t kExpectedPsramSize = 8u * 1024u * 1024u;
constexpr size_t kPsramAllocationThreshold = 4096u;
constexpr uintptr_t kPsramHeapBase = 0x11000000u;
constexpr size_t kPsramHeapSize = 6u * 1024u * 1024u;

bool g_initialized;
bool g_storage_ready;
bool g_psram_ready;
bool g_video_ready;
bool g_input_ready;
bool g_launched_from_bootloader;
size_t g_psram_size;
lwmem_t g_psram_heap;
lwmem_region_t g_psram_regions[] = {
    {reinterpret_cast<void *>(kPsramHeapBase), kPsramHeapSize},
    {nullptr, 0u}
};
}

struct SDL_Mutex {
    recursive_mutex_t value;
};

extern "C" SDL_Mutex *SDL_CreateMutex(void)
{
    SDL_Mutex *mutex = static_cast<SDL_Mutex *>(compat_malloc(sizeof(*mutex)));
    if (mutex != nullptr)
        recursive_mutex_init(&mutex->value);
    return mutex;
}

extern "C" void SDL_DestroyMutex(SDL_Mutex *mutex)
{
    compat_free(mutex);
}

extern "C" bool SDL_LockMutex(SDL_Mutex *mutex)
{
    if (mutex == nullptr)
        return false;
    recursive_mutex_enter_blocking(&mutex->value);
    return true;
}

extern "C" bool SDL_UnlockMutex(SDL_Mutex *mutex)
{
    if (mutex == nullptr)
        return false;
    recursive_mutex_exit(&mutex->value);
    return true;
}

extern "C" bool SDL_TryLockMutex(SDL_Mutex *mutex)
{
    return mutex != nullptr && recursive_mutex_try_enter(&mutex->value, nullptr);
}

extern "C" void SDL_Delay(uint32_t ms) { compat_delay_ms(ms); }
extern "C" uint32_t SDL_GetTicks(void) { return compat_ticks_ms(); }
extern "C" uint64_t SDL_GetTicksNS(void) { return compat_ticks_us() * 1000u; }

extern "C" int compat_init(void)
{
    if (g_initialized) {
        return g_storage_ready && g_psram_ready && g_video_ready &&
                       g_input_ready && g_psram_size == kExpectedPsramSize
                   ? 0
                   : -1;
    }

    /* Keep clock/QMI/PSRAM ownership in the main-window shared layer.  The
     * port only sequences its public operations and never writes QMI timing or
     * reimplements psram_reinitialize(). */
    Frens::setClocksAndStartStdio(kSystemClockKHz,
                                 vreg_voltage::VREG_VOLTAGE_1_20);
    g_launched_from_bootloader = Frens::isLaunchedFromBootloader();

#if RETROPICOES_BOOTLOADER_IMAGE
    printf("REFKEEN BUILD: s20-c34 HW16 Loader image (video trace)\n");
#else
    printf("REFKEEN BUILD: s20-c33 HW16 standalone image (link wrap)\n");
#endif
    printf("REFKEEN BOOT: launched_by_loader=%d clock=%u\n",
           g_launched_from_bootloader ? 1 : 0,
           static_cast<unsigned>(clock_get_hz(clk_sys)));

    Frens::configQspiPads();
    g_psram_ready = Frens::initPsram();
    g_psram_size = g_psram_ready ? psram_get_size() : 0u;
    printf("REFKEEN PSRAM: ready=%d size=%u expected=%u\n",
           g_psram_ready ? 1 : 0,
           static_cast<unsigned>(g_psram_size),
           static_cast<unsigned>(kExpectedPsramSize));

    if (!g_psram_ready || g_psram_size != kExpectedPsramSize) {
        printf("REFKEEN ERROR: PSRAM unavailable\n");
        stdio_flush();
        if (g_launched_from_bootloader) {
            sleep_ms(250);
            Frens::rebootToBootloader();
        }
        return -1;
    }
    if (lwmem_assignmem_ex(&g_psram_heap, g_psram_regions) == 0u) {
        printf("REFKEEN ERROR: PSRAM heap initialization failed\n");
        return -1;
    }

    g_storage_ready = Frens::initSDCard();
    g_video_ready = g_storage_ready && retropicoes_video_init() == 0;
    g_input_ready = g_video_ready && retropicoes_input_init() == 0;
    g_initialized = true;

    printf("REFKEEN INIT: sd=%d video=%d input=%d\n",
           g_storage_ready ? 1 : 0,
           g_video_ready ? 1 : 0,
           g_input_ready ? 1 : 0);

    return g_storage_ready && g_video_ready && g_input_ready ? 0 : -1;
}

extern "C" void compat_shutdown(void)
{
    compat_audio_stop();
    if (g_input_ready)
        retropicoes_input_shutdown();
    if (g_video_ready)
        retropicoes_video_shutdown();
    if (g_storage_ready)
        (void)f_unmount("");

    g_input_ready = false;
    g_video_ready = false;
    g_storage_ready = false;
    g_initialized = false;
}

extern "C" uint32_t compat_ticks_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

extern "C" uint64_t compat_ticks_us(void)
{
    return time_us_64();
}

extern "C" void compat_delay_ms(uint32_t milliseconds)
{
    sleep_ms(milliseconds);
}

extern "C" void *compat_malloc(size_t size)
{
    if (size == 0u)
        return nullptr;
    if (g_psram_ready && size >= kPsramAllocationThreshold)
        return lwmem_malloc_ex(&g_psram_heap, nullptr, size);
    return std::malloc(size);
}

extern "C" void *compat_calloc(size_t count, size_t size)
{
    if (count == 0u || size == 0u ||
        size > std::numeric_limits<size_t>::max() / count)
        return nullptr;

    const size_t total = count * size;
    void *ptr = compat_malloc(total);
    if (ptr != nullptr)
        std::memset(ptr, 0, total);
    return ptr;
}

extern "C" void *compat_large_malloc(size_t size)
{
    if (size == 0u)
        return nullptr;
    return g_psram_ready ? lwmem_malloc_ex(&g_psram_heap, nullptr, size)
                         : std::malloc(size);
}

extern "C" void *compat_large_calloc(size_t count, size_t size)
{
    if (count == 0u || size == 0u ||
        size > std::numeric_limits<size_t>::max() / count)
        return nullptr;
    const size_t total = count * size;
    void *ptr = compat_large_malloc(total);
    if (ptr != nullptr)
        std::memset(ptr, 0, total);
    return ptr;
}

extern "C" void compat_free(void *ptr)
{
    if (ptr == nullptr)
        return;
    if (g_psram_ready && psram_check_address(ptr))
        lwmem_free_ex(&g_psram_heap, ptr);
    else
        std::free(ptr);
}

extern "C" void *retropicoes_refkeen_game_malloc(size_t size)
{
    void *ptr = compat_large_malloc(size);
    if (ptr == nullptr && size != 0u)
        printf("REFKEEN HEAP: PSRAM malloc failed size=%u\n",
               static_cast<unsigned>(size));
    return ptr;
}

extern "C" void *retropicoes_refkeen_game_calloc(size_t count, size_t size)
{
    void *ptr = compat_large_calloc(count, size);
    if (ptr == nullptr && count != 0u && size != 0u)
        printf("REFKEEN HEAP: PSRAM calloc failed count=%u size=%u\n",
               static_cast<unsigned>(count), static_cast<unsigned>(size));
    return ptr;
}

extern "C" void *retropicoes_refkeen_game_realloc(void *ptr, size_t size)
{
    if (ptr == nullptr)
        return retropicoes_refkeen_game_malloc(size);
    if (size == 0u) {
        compat_free(ptr);
        return nullptr;
    }
    void *result = g_psram_ready && psram_check_address(ptr)
        ? lwmem_realloc_ex(&g_psram_heap, nullptr, ptr, size)
        : std::realloc(ptr, size);
    if (result == nullptr)
        printf("REFKEEN HEAP: realloc failed size=%u\n",
               static_cast<unsigned>(size));
    return result;
}

extern "C" void retropicoes_refkeen_game_free(void *ptr)
{
    compat_free(ptr);
}

extern "C" int retropicoes_storage_ready(void)
{
    return g_storage_ready ? 1 : 0;
}

extern "C" int retropicoes_psram_ready(void)
{
    return g_psram_ready ? 1 : 0;
}

extern "C" size_t retropicoes_psram_size(void)
{
    return g_psram_size;
}

extern "C" uint32_t retropicoes_system_clock_hz(void)
{
    return clock_get_hz(clk_sys);
}

extern "C" int retropicoes_pointer_in_psram(const void *ptr)
{
    return ptr != nullptr && g_psram_ready &&
                   psram_check_address(const_cast<void *>(ptr))
               ? 1
               : 0;
}

extern "C" int retropicoes_launched_from_bootloader(void)
{
    return g_launched_from_bootloader ? 1 : 0;
}

extern "C" void retropicoes_return_to_bootloader(void)
{
    printf("REFKEEN LOADER: return requested\n");
    stdio_flush();
    Frens::rebootToBootloader();
}
