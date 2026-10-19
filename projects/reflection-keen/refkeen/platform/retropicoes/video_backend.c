#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "backend/video/be_video.h"
#include "backend/video/be_video_emu.h"
#include "backend/video/be_video_textures.h"
#include "backend/video/be_video_ui.h"
#include "be_cross_mem_internal.h"
#include "be_st_cfg.h"
#include "compat.h"
#include "indexed_video.h"
#include "stdio_compat.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "runtime.h"
#include "hstx_data_island_queue.h"
#include "video_output.h"

typedef struct {
    int width;
    int height;
    uint32_t *pixels;
} pico_texture_t;

enum {
    FRAME_PIXELS = COMPAT_SCREEN_WIDTH * COMPAT_SCREEN_HEIGHT,
    SIGNAL_WIDTH = COMPAT_SCREEN_WIDTH * 2,
    SIGNAL_HEIGHT = COMPAT_SCREEN_HEIGHT * 2,
    CORE1_STACK_WORDS = 1024
};

static compat_pixel_t *g_draw;
static compat_pixel_t g_scanout[2][FRAME_PIXELS] __attribute__((aligned(4)));
static uint32_t g_core1_stack[CORE1_STACK_WORDS] __attribute__((aligned(8)));
static compat_framebuffer_t g_framebuffer;
static volatile unsigned g_active;
static volatile unsigned g_pending;
static volatile bool g_ready;
static volatile bool g_scanlines;
static volatile uint32_t g_vsync_count;
static volatile uint32_t g_display_count;
static bool g_video_ready;
static bool g_draw_acquired;
static unsigned g_draw_index;
static uint64_t g_buffer_wait_total_us;
static uint32_t g_buffer_wait_peak_us;
static uint32_t g_buffer_wait_count;
static pico_texture_t *g_render_target;
static uint32_t g_draw_color;
static uint64_t g_render_total_us;
static uint32_t g_render_peak_us;
static uint64_t g_source_lock_start_us;
static uint64_t g_source_upload_total_us;
static uint32_t g_source_upload_count;
static uint32_t g_indexed_fast_frames;
static uint32_t g_text_rows[3][720];
typedef struct {
    uint16_t first;
    uint16_t count;
    uint16_t weight[4];
} text_x_span_t;
typedef struct {
    uint16_t first;
    uint16_t count;
    uint16_t weight[3];
} text_y_span_t;
static text_x_span_t g_text_x_spans[COMPAT_SCREEN_WIDTH];
static text_y_span_t g_text_y_spans[COMPAT_SCREEN_HEIGHT];
static int g_text_span_width;
static int g_text_span_height;
static uint32_t g_scaled_row[640];
static uint16_t g_scaled_x[COMPAT_SCREEN_WIDTH];
extern uint8_t __refkeen_psram_bss_start;
extern uint8_t __refkeen_psram_bss_end;

static inline uint16_t rgb565(uint32_t bgra)
{
    return (uint16_t)((((bgra >> 16) & 0xf8u) << 8) |
                      (((bgra >> 8) & 0xfcu) << 3) |
                      ((bgra & 0xf8u) >> 3));
}

static void __not_in_flash_func(scanline_callback)(
    uint32_t v_scanline, uint32_t active_line, uint32_t *line)
{
    (void)v_scanline;
    if (active_line >= SIGNAL_HEIGHT ||
        (g_scanlines && (active_line & 1u))) {
        memset(line, 0, SIGNAL_WIDTH * sizeof(uint16_t));
        return;
    }
    const uint32_t *src = (const uint32_t *)&g_scanout[g_active]
        [(active_line >> 1) * COMPAT_SCREEN_WIDTH];
    for (unsigned x = 0; x < COMPAT_SCREEN_WIDTH / 2u; ++x) {
        const uint32_t pair = src[x];
        const uint32_t p0 = pair & 0xffffu;
        const uint32_t p1 = pair >> 16;
        line[x * 2u] = p0 | (p0 << 16);
        line[x * 2u + 1u] = p1 | (p1 << 16);
    }
}

static void __not_in_flash_func(vsync_callback)(void)
{
    ++g_vsync_count;
    if (g_ready) {
        g_active = g_pending;
        __dmb();
        g_ready = false;
        ++g_display_count;
    }
}

static void acquire_draw_buffer(void)
{
    if (!g_video_ready || g_draw_acquired)
        return;
    const uint64_t start_us = time_us_64();
    uint64_t next_diagnostic_us = start_us + 1000000u;
    while (g_ready) {
        const uint64_t now_us = time_us_64();
        if (now_us >= next_diagnostic_us) {
            printf("REFKEEN VIDEO WAIT: us=%u vsync=%u active=%u pending=%u ready=%u\n",
                   (unsigned)(now_us - start_us), (unsigned)g_vsync_count,
                   (unsigned)g_active, (unsigned)g_pending,
                   (unsigned)g_ready);
            next_diagnostic_us = now_us + 1000000u;
        }
        tight_loop_contents();
    }
    __dmb();
    g_draw_index = g_active ^ 1u;
    g_draw = g_scanout[g_draw_index];
    g_framebuffer.pixels = g_draw;
    g_draw_acquired = true;
    const uint32_t wait_us = (uint32_t)(time_us_64() - start_us);
    g_buffer_wait_total_us += wait_us;
    if (wait_us > g_buffer_wait_peak_us)
        g_buffer_wait_peak_us = wait_us;
    ++g_buffer_wait_count;
}

int retropicoes_video_init(void)
{
    if (g_video_ready)
        return 0;
    if (clock_get_hz(clk_sys) != 252000000u)
        return -1;
    g_draw = NULL;
    g_draw_acquired = false;
    g_framebuffer.pixels = NULL;
    g_framebuffer.stride = COMPAT_SCREEN_WIDTH;
    g_framebuffer.width = COMPAT_SCREEN_WIDTH;
    g_framebuffer.height = COMPAT_SCREEN_HEIGHT;
    memset(&__refkeen_psram_bss_start, 0,
           (size_t)(&__refkeen_psram_bss_end -
                    &__refkeen_psram_bss_start));
    memset(g_scanout, 0, sizeof(g_scanout));
    for (unsigned y = 0; y < COMPAT_SCREEN_HEIGHT; ++y) {
        const uint16_t color = y < COMPAT_SCREEN_HEIGHT / 3u ? 0xf800u
            : y < (COMPAT_SCREEN_HEIGHT * 2u) / 3u ? 0x07e0u : 0x001fu;
        for (unsigned x = 0; x < COMPAT_SCREEN_WIDTH; ++x)
            g_scanout[0][y * COMPAT_SCREEN_WIDTH + x] = color;
    }
    g_active = 0u;
    g_pending = 1u;
    g_ready = false;
    g_scanlines = false;
    clock_configure_int_divider(clk_hstx, 0u,
        CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLK_SYS, 252000000u, 2u);
    hstx_di_queue_init();
    video_output_set_dvi_mode(FORCE_DVI != 0);
    video_output_set_vsync_callback(vsync_callback);
    video_output_init(SIGNAL_WIDTH, SIGNAL_HEIGHT);
    video_output_set_scanline_callback(scanline_callback);
    g_video_ready = true;
    multicore_launch_core1_with_stack(video_output_core1_run, g_core1_stack,
                                      sizeof(g_core1_stack));
    printf("REFKEEN VIDEO DIAG: RGB bars begin, hstx=%u Hz\n",
           (unsigned)clock_get_hz(clk_hstx));
    sleep_ms(2000u);
    printf("REFKEEN VIDEO DIAG: RGB bars end, vsync=%u\n",
           (unsigned)g_vsync_count);
    return 0;
}

void retropicoes_video_shutdown(void)
{
    if (!g_video_ready)
        return;
    video_output_set_background_task(NULL);
    video_output_stop();
    multicore_reset_core1();
    g_video_ready = false;
    g_draw = NULL;
    g_draw_acquired = false;
    memset(&g_framebuffer, 0, sizeof(g_framebuffer));
}

compat_framebuffer_t *compat_framebuffer(void)
{
    if (!g_video_ready)
        return NULL;
    acquire_draw_buffer();
    return &g_framebuffer;
}

void compat_present(void)
{
    static unsigned present_count;
    static uint64_t window_start_us;
    static uint64_t submit_total_us;
    static uint32_t submit_peak_us;
    static uint32_t window_presents;
    static uint32_t last_display_count;
    static uint32_t last_vsync_count;
    if (!g_video_ready)
        return;
    acquire_draw_buffer();
    const uint64_t start_us = time_us_64();
    ++present_count;
    if (present_count <= 3u || present_count == 60u) {
        unsigned nonzero = 0u;
        for (unsigned i = 0; i < FRAME_PIXELS; ++i)
            nonzero += g_draw[i] != 0u;
        printf("REFKEEN VIDEO DIAG: present=%u nonzero=%u center=%04x vsync=%u\n",
               present_count, nonzero,
               (unsigned)g_draw[FRAME_PIXELS / 2u],
               (unsigned)g_vsync_count);
    }
    g_pending = g_draw_index;
    __dmb();
    g_ready = true;
    g_draw_acquired = false;
    const uint32_t submit_us = (uint32_t)(time_us_64() - start_us);
    submit_total_us += submit_us;
    if (submit_us > submit_peak_us)
        submit_peak_us = submit_us;
    ++window_presents;
    if (!window_start_us) {
        window_start_us = start_us;
        last_display_count = g_display_count;
        last_vsync_count = g_vsync_count;
    }
    const uint64_t elapsed_us = time_us_64() - window_start_us;
    if (elapsed_us >= 5000000u) {
        const uint32_t displayed = g_display_count;
        const uint32_t vsyncs = g_vsync_count;
        const unsigned present_fps_tenths = (unsigned)
            (((uint64_t)window_presents * 10000000u + elapsed_us / 2u) /
             elapsed_us);
        const unsigned display_fps_tenths = (unsigned)
            (((uint64_t)(displayed - last_display_count) * 10000000u +
              elapsed_us / 2u) / elapsed_us);
        printf("REFKEEN PERF: interval_ms=%u present=%u display=%u vsync=%u "
               "present_fps=%u.%u display_fps=%u.%u "
               "present_submit_avg_us=%u present_submit_peak_us=%u "
               "buffer_wait_avg_us=%u buffer_wait_peak_us=%u "
               "source_updates=%u source_upload_avg_us=%u "
               "indexed_frames=%u "
               "render_avg_us=%u render_peak_us=%u\n",
               (unsigned)(elapsed_us / 1000u), (unsigned)window_presents,
               (unsigned)(displayed - last_display_count),
               (unsigned)(vsyncs - last_vsync_count),
               present_fps_tenths / 10u, present_fps_tenths % 10u,
               display_fps_tenths / 10u, display_fps_tenths % 10u,
               (unsigned)(submit_total_us / window_presents),
               (unsigned)submit_peak_us,
               (unsigned)(g_buffer_wait_count ?
                   g_buffer_wait_total_us / g_buffer_wait_count : 0u),
               (unsigned)g_buffer_wait_peak_us,
               (unsigned)g_source_upload_count,
               (unsigned)(g_source_upload_count ?
                   g_source_upload_total_us / g_source_upload_count : 0u),
               (unsigned)g_indexed_fast_frames,
               (unsigned)(g_render_total_us / window_presents),
               (unsigned)g_render_peak_us);
        window_start_us = time_us_64();
        window_presents = 0u;
        submit_total_us = 0u;
        submit_peak_us = 0u;
        g_buffer_wait_total_us = 0u;
        g_buffer_wait_peak_us = 0u;
        g_buffer_wait_count = 0u;
        g_source_upload_total_us = 0u;
        g_source_upload_count = 0u;
        g_indexed_fast_frames = 0u;
        g_render_total_us = 0u;
        g_render_peak_us = 0u;
        last_display_count = displayed;
        last_vsync_count = vsyncs;
    }
}

void retropicoes_video_set_scanlines(int enabled)
{
    g_scanlines = enabled != 0;
}

void BEL_ST_RecreateWindowAndRenderer(int w, int h, int fw, int fh,
    float density, bool fullscreen, bool resizable, bool vsync,
    const char *driver)
{
    (void)w; (void)h; (void)fw; (void)fh; (void)density;
    (void)fullscreen; (void)resizable; (void)vsync; (void)driver;
}

void BEL_ST_DestroyWindowAndRenderer(void) {}

BE_ST_Texture *BEL_ST_CreateARGBTexture(int w, int h, bool target, bool linear)
{
    (void)target; (void)linear;
    if (w <= 0 || h <= 0 || (size_t)w > SIZE_MAX / (size_t)h / 4u)
        return NULL;
    pico_texture_t *texture = compat_large_calloc(1u, sizeof(*texture));
    if (!texture)
        return NULL;
    texture->pixels = compat_large_calloc((size_t)w * (size_t)h,
                                           sizeof(*texture->pixels));
    if (!texture->pixels) {
        compat_free(texture);
        return NULL;
    }
    texture->width = w;
    texture->height = h;
    return (BE_ST_Texture *)texture;
}

void BEL_ST_DestroyTexture(BE_ST_Texture *opaque)
{
    pico_texture_t *texture = (pico_texture_t *)opaque;
    if (!texture) return;
    if (g_render_target == texture) g_render_target = NULL;
    compat_free(texture->pixels);
    compat_free(texture);
}

void *BEL_ST_LockTexture(BE_ST_Texture *texture)
{
    if (texture && texture == g_sdlTexture)
        g_source_lock_start_us = time_us_64();
    return texture ? ((pico_texture_t *)texture)->pixels : NULL;
}

void BEL_ST_UnlockTexture(BE_ST_Texture *texture)
{
    if (texture && texture == g_sdlTexture && g_source_lock_start_us) {
        g_source_upload_total_us += time_us_64() - g_source_lock_start_us;
        ++g_source_upload_count;
        g_source_lock_start_us = 0u;
    }
}

void BEL_ST_UpdateTexture(BE_ST_Texture *opaque, const BE_ST_Rect *rect,
                          const void *pixels, int pitch)
{
    pico_texture_t *texture = (pico_texture_t *)opaque;
    if (!texture || !pixels) return;
    const int x = rect ? rect->x : 0, y = rect ? rect->y : 0;
    const int w = rect ? rect->w : texture->width;
    const int h = rect ? rect->h : texture->height;
    for (int row = 0; row < h; ++row)
        memcpy(texture->pixels + (y + row) * texture->width + x,
               (const uint8_t *)pixels + row * pitch, (size_t)w * 4u);
    if (opaque == g_sdlTexture)
        ++g_source_upload_count;
}

void BEL_ST_SetTextureBlendMode(BE_ST_Texture *texture, bool blend)
{ (void)texture; (void)blend; }

bool BEL_ST_SetRenderTarget(BE_ST_Texture *texture)
{
    g_render_target = (pico_texture_t *)texture;
    return true;
}

static void render_scaled(pico_texture_t *source, uint32_t *dst32,
                          int dst_w, int dst_h, const BE_ST_Rect *rect)
{
    int dx = rect ? rect->x : 0, dy = rect ? rect->y : 0;
    int dw = rect ? rect->w : dst_w, dh = rect ? rect->h : dst_h;
    if (dw <= 0 || dh <= 0) return;
    for (int y = 0; y < dh; ++y) {
        int yy = dy + y;
        if ((unsigned)yy >= (unsigned)dst_h) continue;
        const uint32_t *src = source->pixels +
            (size_t)(y * source->height / dh) * source->width;
        for (int x = 0; x < dw; ++x) {
            int xx = dx + x;
            if ((unsigned)xx < (unsigned)dst_w)
                dst32[(size_t)yy * dst_w + xx] =
                    src[x * source->width / dw];
        }
    }
}

static void __not_in_flash_func(render_text_coverage)(
    const pico_texture_t *source, int dx, int dy, int dw, int dh)
{
    const int sw = source->width, sh = source->height;
    if (dw >= 240) {
        if (dw != g_text_span_width || dh != g_text_span_height) {
            for (int x = 0; x < dw; ++x) {
                const int left = x * sw, right = (x + 1) * sw;
                text_x_span_t *span = &g_text_x_spans[x];
                span->first = (uint16_t)(left / dw);
                span->count = (uint16_t)((right + dw - 1) / dw - span->first);
                for (unsigned i = 0; i < span->count; ++i) {
                    const int sx = span->first + (int)i;
                    const int x0 = left > sx * dw ? left : sx * dw;
                    const int x1 = right < (sx + 1) * dw ? right : (sx + 1) * dw;
                    span->weight[i] = (uint16_t)(x1 - x0);
                }
            }
            for (int y = 0; y < dh; ++y) {
                const int top = y * sh, bottom = (y + 1) * sh;
                text_y_span_t *span = &g_text_y_spans[y];
                span->first = (uint16_t)(top / dh);
                span->count = (uint16_t)((bottom + dh - 1) / dh - span->first);
                for (unsigned i = 0; i < span->count; ++i) {
                    const int sy = span->first + (int)i;
                    const int y0 = top > sy * dh ? top : sy * dh;
                    const int y1 = bottom < (sy + 1) * dh ? bottom : (sy + 1) * dh;
                    span->weight[i] = (uint16_t)(y1 - y0);
                }
            }
            g_text_span_width = dw;
            g_text_span_height = dh;
        }
        for (int y = 0; y < dh; ++y) {
            const int yy = dy + y;
            if ((unsigned)yy >= COMPAT_SCREEN_HEIGHT) continue;
            const text_y_span_t *yspan = &g_text_y_spans[y];
            for (unsigned i = 0; i < yspan->count; ++i)
                memcpy(g_text_rows[i],
                       source->pixels + (size_t)(yspan->first + i) * sw,
                       (size_t)sw * sizeof(uint32_t));
            for (int x = 0; x < dw; ++x) {
                const int xx = dx + x;
                if ((unsigned)xx >= COMPAT_SCREEN_WIDTH) continue;
                const text_x_span_t *xspan = &g_text_x_spans[x];
                uint32_t red = 0, green = 0, blue = 0;
                uint32_t darkest = 0;
                unsigned darkest_luma = UINT_MAX, brightest_luma = 0;
                for (unsigned yi = 0; yi < yspan->count; ++yi) {
                    const unsigned wy = yspan->weight[yi];
                    const uint32_t *row = g_text_rows[yi] + xspan->first;
                    for (unsigned xi = 0; xi < xspan->count; ++xi) {
                        const unsigned w = wy * xspan->weight[xi];
                        const uint32_t color = row[xi];
                        const unsigned r = (color >> 16) & 255u;
                        const unsigned g = (color >> 8) & 255u;
                        const unsigned b = color & 255u;
                        const unsigned luma = 3u * r + 6u * g + b;
                        if (luma < darkest_luma) {
                            darkest_luma = luma;
                            darkest = color;
                        }
                        if (luma > brightest_luma) brightest_luma = luma;
                        red += r * w;
                        green += g * w;
                        blue += b * w;
                    }
                }
                red /= (unsigned)(sw * sh);
                green /= (unsigned)(sw * sh);
                blue /= (unsigned)(sw * sh);
                if (brightest_luma - darkest_luma >= 960u) {
                    red = (3u * red + ((darkest >> 16) & 255u)) / 4u;
                    green = (3u * green + ((darkest >> 8) & 255u)) / 4u;
                    blue = (3u * blue + (darkest & 255u)) / 4u;
                }
                const uint32_t color = 0xff000000u |
                    (red << 16) | (green << 8) | blue;
                g_draw[(size_t)yy * COMPAT_SCREEN_WIDTH + xx] = rgb565(color);
            }
        }
        return;
    }
    for (int y = 0; y < dh; ++y) {
        const int yy = dy + y;
        if ((unsigned)yy >= COMPAT_SCREEN_HEIGHT) continue;
        const int top = y * sh, bottom = (y + 1) * sh;
        const int first_sy = top / dh;
        const int end_sy = (bottom + dh - 1) / dh;
        for (int sy = first_sy; sy < end_sy; ++sy)
            memcpy(g_text_rows[sy - first_sy],
                   source->pixels + (size_t)sy * sw,
                   (size_t)sw * sizeof(uint32_t));
        for (int x = 0; x < dw; ++x) {
            const int xx = dx + x;
            if ((unsigned)xx >= COMPAT_SCREEN_WIDTH) continue;
            const int left = x * sw, right = (x + 1) * sw;
            uint32_t red = 0, green = 0, blue = 0, weight = 0;
            uint32_t darkest = 0;
            unsigned darkest_luma = UINT_MAX, brightest_luma = 0;
            for (int sy = first_sy; sy < end_sy; ++sy) {
                const int y0 = top > sy * dh ? top : sy * dh;
                const int y1 = bottom < (sy + 1) * dh ? bottom : (sy + 1) * dh;
                const int wy = y1 - y0;
                for (int sx = left / dw; sx < (right + dw - 1) / dw; ++sx) {
                    const int x0 = left > sx * dw ? left : sx * dw;
                    const int x1 = right < (sx + 1) * dw ? right : (sx + 1) * dw;
                    const unsigned w = (unsigned)(wy * (x1 - x0));
                    const uint32_t color = g_text_rows[sy - first_sy][sx];
                    const unsigned r = (color >> 16) & 255u;
                    const unsigned g = (color >> 8) & 255u;
                    const unsigned b = color & 255u;
                    const unsigned luma = 3u * r + 6u * g + b;
                    if (luma < darkest_luma) {
                        darkest_luma = luma;
                        darkest = color;
                    }
                    if (luma > brightest_luma) brightest_luma = luma;
                    red += r * w;
                    green += g * w;
                    blue += b * w;
                    weight += w;
                }
            }
            red /= weight;
            green /= weight;
            blue /= weight;
            if (brightest_luma - darkest_luma >= 960u) {
                red = (3u * red + ((darkest >> 16) & 255u)) / 4u;
                green = (3u * green + ((darkest >> 8) & 255u)) / 4u;
                blue = (3u * blue + (darkest & 255u)) / 4u;
            }
            const uint32_t color = 0xff000000u |
                (red << 16) | (green << 8) | blue;
            g_draw[(size_t)yy * COMPAT_SCREEN_WIDTH + xx] = rgb565(color);
        }
    }
}

bool __not_in_flash_func(BEL_ST_RenderFromTexture)(
    BE_ST_Texture *opaque, const BE_ST_Rect *rect)
{
    pico_texture_t *source = (pico_texture_t *)opaque;
    if (!source) return false;
    if (!g_render_target)
        acquire_draw_buffer();
    static int last_sw, last_sh, last_dw, last_dh;
    const uint64_t start_us = time_us_64();
    if (g_render_target) {
        render_scaled(source, g_render_target->pixels,
                      g_render_target->width, g_render_target->height, rect);
        goto done;
    }
    int dx = rect ? rect->x : 0, dy = rect ? rect->y : 0;
    int dw = rect ? rect->w : (int)COMPAT_SCREEN_WIDTH;
    int dh = rect ? rect->h : (int)COMPAT_SCREEN_HEIGHT;
    if (last_sw != source->width || last_sh != source->height ||
        last_dw != dw || last_dh != dh) {
        printf("REFKEEN VIDEO SCALE: source=%dx%d dest=%dx%d offset=%d,%d\n",
               source->width, source->height, dw, dh, dx, dy);
        last_sw = source->width;
        last_sh = source->height;
        last_dw = dw;
        last_dh = dh;
    }
    if (source->width == 720 && source->height == 400 &&
        dw > 0 && dw <= (int)COMPAT_SCREEN_WIDTH &&
        dh >= 200 && dh <= (int)COMPAT_SCREEN_HEIGHT) {
        render_text_coverage(source, dx, dy, dw, dh);
        goto done;
    }
    if (source->width <= 640 && dw > 0 &&
        dw <= (int)COMPAT_SCREEN_WIDTH &&
        dh > 0) {
        for (int x = 0; x < dw; ++x)
            g_scaled_x[x] = (uint16_t)(x * source->width / dw);
        int last_source_y = -1;
        for (int y = 0; y < dh; ++y) {
            const int yy = dy + y;
            if ((unsigned)yy >= COMPAT_SCREEN_HEIGHT) continue;
            const int sy = y * source->height / dh;
            if (sy != last_source_y) {
                memcpy(g_scaled_row,
                       source->pixels + (size_t)sy * source->width,
                       (size_t)source->width * sizeof(uint32_t));
                last_source_y = sy;
            }
            if ((dx & 1) == 0 && (dw & 1) == 0 && dx >= 0 &&
                dx + dw <= (int)COMPAT_SCREEN_WIDTH) {
                uint32_t *dst = (uint32_t *)(g_draw +
                    (size_t)yy * COMPAT_SCREEN_WIDTH + dx);
                for (int x = 0; x < dw; x += 2) {
                    const uint32_t first = rgb565(g_scaled_row[g_scaled_x[x]]);
                    const uint32_t second = rgb565(g_scaled_row[g_scaled_x[x + 1]]);
                    dst[x / 2] = first | (second << 16);
                }
                continue;
            }
            for (int x = 0; x < dw; ++x) {
                const int xx = dx + x;
                if ((unsigned)xx < COMPAT_SCREEN_WIDTH)
                    g_draw[(size_t)yy * COMPAT_SCREEN_WIDTH + xx] =
                        rgb565(g_scaled_row[g_scaled_x[x]]);
            }
        }
        goto done;
    }
    for (int y = 0; y < dh; ++y) {
        int yy = dy + y;
        if ((unsigned)yy >= COMPAT_SCREEN_HEIGHT) continue;
        const uint32_t *src = source->pixels +
            (size_t)(y * source->height / dh) * source->width;
        for (int x = 0; x < dw; ++x) {
            int xx = dx + x;
            if ((unsigned)xx < COMPAT_SCREEN_WIDTH)
                g_draw[(size_t)yy * COMPAT_SCREEN_WIDTH + xx] =
                    rgb565(src[x * source->width / dw]);
        }
    }
done:
    {
        const uint32_t elapsed_us = (uint32_t)(time_us_64() - start_us);
        g_render_total_us += elapsed_us;
        if (elapsed_us > g_render_peak_us) g_render_peak_us = elapsed_us;
    }
    return true;
}

void BEL_ST_SetDrawColor(uint32_t color) { g_draw_color = color; }
void BEL_ST_SetDrawBlendMode(bool blend) { (void)blend; }

void BEL_ST_RenderClear(void)
{
    if (g_render_target)
        for (int i = 0; i < g_render_target->width * g_render_target->height; ++i)
            g_render_target->pixels[i] = g_draw_color;
    else {
        acquire_draw_buffer();
        uint16_t color = rgb565(g_draw_color);
        for (unsigned i = 0; i < FRAME_PIXELS; ++i) g_draw[i] = color;
    }
}

void BEL_ST_RenderFill(const BE_ST_Rect *r)
{
    if (!r) return;
    if (g_render_target) {
        for (int y = 0; y < r->h; ++y) for (int x = 0; x < r->w; ++x) {
            int xx = r->x + x, yy = r->y + y;
            if ((unsigned)xx < (unsigned)g_render_target->width &&
                (unsigned)yy < (unsigned)g_render_target->height)
                g_render_target->pixels[yy * g_render_target->width + xx] = g_draw_color;
        }
    } else {
        acquire_draw_buffer();
        uint16_t color = rgb565(g_draw_color);
        for (int y = 0; y < r->h; ++y) for (int x = 0; x < r->w; ++x) {
            int xx = r->x + x, yy = r->y + y;
            if ((unsigned)xx < COMPAT_SCREEN_WIDTH && (unsigned)yy < COMPAT_SCREEN_HEIGHT)
                g_draw[yy * COMPAT_SCREEN_WIDTH + xx] = color;
        }
    }
}

#if REFKEEN_INDEXED_FAST_PATH
extern BE_ST_Rect g_sdlAspectCorrectionRect;
extern BE_ST_Rect g_sdlAspectCorrectionBorderedRect;
void __real_BEL_ST_UpdateHostDisplay(void);

void __wrap_BEL_ST_UpdateHostDisplay(void)
{
    static uint32_t call_count;
    const uint32_t call = ++call_count;
    if (call <= 12u)
        printf("REFKEEN VIDEO CALL: enter=%u mode=%d ms=%u\n",
               (unsigned)call, g_sdlScreenMode,
               (unsigned)compat_ticks_ms());
    static bool was_fast;
    static bool graphics_fallback_logged;
    static bool text_fallback_logged;
    static bool palette_valid;
    static uint32_t last_refresh_ms;
    static uint32_t previous_palette[256];
    static uint16_t palette565[256];
    static uint32_t row_bases[200];
    const bool graphics_mode = g_sdlScreenMode == 0xD ||
                               g_sdlScreenMode == 0xE ||
                               g_sdlScreenMode == 0x13;
    const bool fast = graphics_mode && g_sdlTexWidth == 320 &&
                      g_sdlTexHeight == 200 && !g_sdlTargetTexture &&
                      !g_sdlDoAbsMouseMotion && !g_sdlShowTouchUI &&
                      !g_sdlShowControllerUI && !g_sdlDebugKeysUIIsShown &&
                      !g_refKeenCfg.touchInputDebugging &&
                      g_sdlAspectCorrectionRect.w > 0 &&
                      g_sdlAspectCorrectionRect.w <= (int)COMPAT_SCREEN_WIDTH &&
                      g_sdlAspectCorrectionRect.h > 0;
    if (!fast) {
        const uint64_t fallback_start_us = time_us_64();
        if (!text_fallback_logged && !graphics_mode) {
            printf("REFKEEN VIDEO: text fallback begin mode=%d\n",
                   g_sdlScreenMode);
            text_fallback_logged = true;
        }
        if (graphics_mode && !graphics_fallback_logged) {
            printf("REFKEEN VIDEO: indexed fallback mode=%d size=%dx%d "
                   "target=%d cursor=%d touch=%d controller=%d debug=%d\n",
                   g_sdlScreenMode, g_sdlTexWidth, g_sdlTexHeight,
                   g_sdlTargetTexture != NULL, g_sdlDoAbsMouseMotion,
                   g_sdlShowTouchUI, g_sdlShowControllerUI,
                   g_sdlDebugKeysUIIsShown);
            graphics_fallback_logged = true;
        }
        if (was_fast) {
            g_sdlDoRefreshGfxOutput = true;
            g_sdlHostScrMemCache.egaGfx[0] ^= 0xffu;
            was_fast = false;
        }
        __real_BEL_ST_UpdateHostDisplay();
        if (call <= 12u)
            printf("REFKEEN VIDEO CALL: exit=%u mode=%d ms=%u\n",
                   (unsigned)call, g_sdlScreenMode,
                   (unsigned)compat_ticks_ms());
        const uint32_t fallback_us =
            (uint32_t)(time_us_64() - fallback_start_us);
        if (fallback_us >= 250000u)
            printf("REFKEEN VIDEO FALLBACK: mode=%d elapsed_us=%u vsync=%u\n",
                   g_sdlScreenMode, (unsigned)fallback_us,
                   (unsigned)g_vsync_count);
        return;
    }
    const uint32_t now_ms = compat_ticks_ms();
    const bool palette_changed = !palette_valid ||
        memcmp(previous_palette, g_sdlEGACurrBGRAPalette,
               sizeof(previous_palette)) != 0;
    if (was_fast && !g_sdlDoRefreshGfxOutput &&
        !g_sdlForceGfxControlUiRefresh && !palette_changed &&
        now_ms - last_refresh_ms < 100u) {
        if (call <= 12u)
            printf("REFKEEN VIDEO CALL: skip=%u mode=%d ms=%u\n",
                   (unsigned)call, g_sdlScreenMode, (unsigned)now_ms);
        return;
    }
    if (!was_fast)
        printf("REFKEEN VIDEO: indexed RGB565 fast path active\n");
    was_fast = true;
    if (palette_changed) {
        memcpy(previous_palette, g_sdlEGACurrBGRAPalette,
               sizeof(previous_palette));
        for (unsigned i = 0; i < 256u; ++i)
            palette565[i] = rgb565(g_sdlEGACurrBGRAPalette[i]);
        palette_valid = true;
    }
    const uint32_t start_us = (g_sdlScreenMode == 0x13 ? 4u : 8u) *
                              g_sdlScreenStartAddress;
    const uint32_t wrap_size = g_sdlScreenMode == 0x13 ? 0x40000u : 0x80000u;
    refkeen_indexed_row_bases(row_bases, start_us, g_sdlPelPanning,
                              (uint32_t)g_sdlPixLineWidth,
                              g_sdlSplitScreenLine, wrap_size);
    const uint64_t render_start_us = time_us_64();
    BEL_ST_SetDrawColor(0xff000000u);
    BEL_ST_RenderClear();
    const uint32_t border_color = g_sdlScreenMode == 0x13 ?
        g_sdlEGACurrBGRAPalette[g_overscanBorderColorIndex] :
        g_sdlEGABGRAScreenColors[g_overscanBorderColorIndex];
    BEL_ST_SetDrawColor(border_color);
    BEL_ST_RenderFill(&g_sdlAspectCorrectionBorderedRect);
    refkeen_indexed_blit(g_sdlVidMem.A000.gfxByByte, wrap_size, row_bases,
                         palette565, g_draw, COMPAT_SCREEN_WIDTH,
                         COMPAT_SCREEN_WIDTH, COMPAT_SCREEN_HEIGHT,
                         g_sdlAspectCorrectionRect.x,
                         g_sdlAspectCorrectionRect.y,
                         g_sdlAspectCorrectionRect.w,
                         g_sdlAspectCorrectionRect.h);
    const uint32_t elapsed_us = (uint32_t)(time_us_64() - render_start_us);
    g_render_total_us += elapsed_us;
    if (elapsed_us > g_render_peak_us) g_render_peak_us = elapsed_us;
    ++g_indexed_fast_frames;
    g_sdlDoRefreshGfxOutput = false;
    g_sdlForceGfxControlUiRefresh = false;
    last_refresh_ms = compat_ticks_ms();
    BEL_ST_UpdateWindow();
    if (call <= 12u)
        printf("REFKEEN VIDEO CALL: exit=%u mode=%d ms=%u\n",
               (unsigned)call, g_sdlScreenMode,
               (unsigned)compat_ticks_ms());
}
#endif

void BEL_ST_UpdateWindow(void) { compat_present(); }
void BEL_ST_SetWindowFullScreenToggle(bool value) { (void)value; }
bool BE_ST_HostGfx_GetFullScreenToggle(void) { return true; }
void BEL_ST_SetWindowSizeInCoords(int w, int h) { (void)w; (void)h; }
void BEL_ST_GetWindowSizeInCoords(int *w, int *h) { *w = 320; *h = 240; }
void BEL_ST_GetWindowSizeInPixels(int *w, int *h) { *w = 320; *h = 240; }
float BEL_ST_GetWindowPixelDensity(void) { return 1.0f; }
void BEL_ST_GetDesktopDisplayDims(int *w, int *h) { *w = 320; *h = 240; }
bool BEL_ST_IsConfiguredForSWRendering(void) { return true; }

const char *SDL_GetRenderDriver(int index) { (void)index; return "software"; }

void BEL_ST_ParseSetting_SDLRendererDriver(int *driver, const char *value)
{
    if (driver)
        *driver = (value && strcmp(value, "auto") != 0) ? 0 : -1;
}

void BEL_ST_SaveSDLRendererDriverToConfig(FILE *fp, const char *key, int driver)
{
    retropicoes_fprintf(fp, "%s=%s\n", key, driver < 0 ? "auto" : "software");
}
