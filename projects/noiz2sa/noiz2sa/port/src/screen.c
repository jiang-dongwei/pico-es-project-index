/*
 * RetroPicoES v2.1 screen adapter for the pinned noiz2sa engine.
 *
 * Replaces the upstream SDL screen.c (excluded from the build). The drawing
 * primitives, layer bookkeeping and palette tables keep the upstream
 * algorithms; only the SDL surface/window/joystick parts are replaced by
 * compat v1 and the 320x240 RGB565 presentation path:
 *
 *   640x480 8bpp composition (lpanel | layer | rpanel)
 *     -> palette lookup (RGB565, brightness applied) with 2:1 downscale
 *     -> compat_framebuffer() -> compat_present()
 *
 * 2:1 downscale: each output pixel covers a 2x2 source block; the default
 * operator keeps the brightest of the four indices so bullets stay visible
 * against the dark background (see the S17 milestones; averaged scaling is
 * selectable for the S17-F hardware comparison).
 *
 * Derived from noiz2sa screen.c, Copyright 2002 Kenta Cho (BSD-2-Clause).
 */
#include "screen.h"
#include "noiz2sa_port_testhooks.h"
#include "noiz2sa_port_target.h"

#include "noiz2sa.h"
#include "attractmanager.h"
#include "clrtbl.h"
#include "degutil.h"
#include "letterrender.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int windowMode = 0;
int brightness = DEFAULT_BRIGHTNESS;

int pitch = LAYER_WIDTH;
int ppitch = PANEL_WIDTH;
int lyrSize = LAYER_WIDTH * LAYER_HEIGHT;

LayerBit *l1buf;
LayerBit *l2buf;
LayerBit *buf;
LayerBit *lpbuf;
LayerBit *rpbuf;
static LayerBit *pbuf;
static int32_t *smokeOfs;

const Uint8 *keys;
SDL_Joystick *stick = NULL;
int buttonReversed = 0;

/* Composed 640x480 index frame: SCREEN_WIDTH x SCREEN_HEIGHT. */
#define VIDEO_W SCREEN_WIDTH
#define VIDEO_H SCREEN_HEIGHT
static LayerBit *video;
#define VIDEO_AT(x, y) (video[(x) + (y) * VIDEO_W])

/* The layer sits in the middle: 160 | 320 | 160. */
#define LAYER_X ((VIDEO_W - LAYER_WIDTH) / 2)
#define LPANEL_X 0
#define RPANEL_X (VIDEO_W - PANEL_WIDTH)

static compat_pixel_t palette565[256];
static uint8_t paletteLuma[256];

/* 2:1 downscale operators for the S17-F hardware comparison. */
#define NOIZ2SA_SCALE_BRIGHTEST 0
#define NOIZ2SA_SCALE_AVERAGE 1
static int downscaleMode = NOIZ2SA_SCALE_BRIGHTEST;

/* Title sprites: 4bpp 40x40 BMPs, index 0 is the transparent key. */
#define SPRITE_NUM 7
static const char *spriteFile[SPRITE_NUM] = {
    "images/title_n.bmp", "images/title_o.bmp", "images/title_i.bmp",
    "images/title_z.bmp", "images/title_2.bmp", "images/title_s.bmp",
    "images/title_a.bmp",
};
static LayerBit *sprite[SPRITE_NUM];
static int spriteW[SPRITE_NUM];
static int spriteH[SPRITE_NUM];

static void initPalette(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        unsigned r = (unsigned)color[i].r * (unsigned)brightness / 256u;
        unsigned g = (unsigned)color[i].g * (unsigned)brightness / 256u;
        unsigned b = (unsigned)color[i].b * (unsigned)brightness / 256u;
        palette565[i] = compat_video_rgb565((uint8_t)r, (uint8_t)g,
                                            (uint8_t)b);
        paletteLuma[i] = (uint8_t)(((unsigned)color[i].r * 77u +
                                    (unsigned)color[i].g * 151u +
                                    (unsigned)color[i].b * 28u) >> 8);
    }
}

/* Layer buffers and the smoke offset table live in PSRAM on the target. */
static void *alloc_large(size_t bytes)
{
    void *ptr = compat_large_malloc(bytes);
    if (ptr == NULL) {
        fprintf(stderr, "noiz2sa: out of memory (%u bytes)\n",
                (unsigned)bytes);
        exit(1);
    }
    memset(ptr, 0, bytes);
    return ptr;
}

static void makeSmokeBuf(void)
{
    int x, y, mx, my;
    smokeOfs = (int32_t *)alloc_large(sizeof(int32_t) * LAYER_WIDTH *
                                      LAYER_HEIGHT);
    for (y = 0; y < LAYER_HEIGHT; y++) {
        for (x = 0; x < LAYER_WIDTH; x++) {
            mx = x + sctbl[(x * 8) & (DIV - 1)] / 128;
            my = y + sctbl[(y * 8) & (DIV - 1)] / 128;
            if (mx < 0 || mx >= LAYER_WIDTH || my < 0 ||
                my >= LAYER_HEIGHT) {
                smokeOfs[x + y * LAYER_WIDTH] = -1;
            } else {
                smokeOfs[x + y * LAYER_WIDTH] = mx + my * LAYER_WIDTH;
            }
        }
    }
}

/* Minimal 4bpp BI_RGB BMP reader for the title sprites. Palette entries are
   mapped to the nearest game palette index (the upstream SDL build used
   SDL_ConvertSurface, i.e. nearest-color matching); source index 0 is the
   transparent key and stays 0. */
static int loadBmp4(const char *name, LayerBit **pixels, int *w, int *h)
{
    uint8_t header[54];
    uint8_t pal[16 * 4];
    compat_file_t *file = compat_file_open(name, "rb");
    uint32_t dataOffset, width, height, bpp, compression, rowBytes, y;
    LayerBit lut[16];
    int i;

    if (file == NULL) {
        fprintf(stderr, "noiz2sa: missing sprite %s\n", name);
        return -1;
    }
    if (compat_file_read(header, 1, sizeof(header), file) != sizeof(header) ||
        header[0] != 'B' || header[1] != 'M') {
        compat_file_close(file);
        return -1;
    }
    dataOffset = (uint32_t)header[10] | ((uint32_t)header[11] << 8) |
                 ((uint32_t)header[12] << 16) | ((uint32_t)header[13] << 24);
    width = (uint32_t)header[18] | ((uint32_t)header[19] << 8) |
            ((uint32_t)header[20] << 16) | ((uint32_t)header[21] << 24);
    height = (uint32_t)header[22] | ((uint32_t)header[23] << 8) |
             ((uint32_t)header[24] << 16) | ((uint32_t)header[25] << 24);
    bpp = (uint16_t)header[28] | ((uint16_t)header[29] << 8);
    compression = (uint32_t)header[30] | ((uint32_t)header[31] << 8) |
                  ((uint32_t)header[32] << 16) |
                  ((uint32_t)header[33] << 24);
    if (bpp != 4 || compression != 0 || width == 0 || width > 256 ||
        height == 0 || height > 256) {
        fprintf(stderr, "noiz2sa: unsupported sprite %s\n", name);
        compat_file_close(file);
        return -1;
    }
    if (compat_file_read(pal, 1, sizeof(pal), file) != sizeof(pal)) {
        compat_file_close(file);
        return -1;
    }
    for (i = 0; i < 16; i++) {
        const uint8_t b = pal[i * 4 + 0];
        const uint8_t g = pal[i * 4 + 1];
        const uint8_t r = pal[i * 4 + 2];
        int best = 0;
        long bestDist = -1;
        int c;
        for (c = 0; c < 256; c++) {
            long dr = (long)r - color[c].r;
            long dg = (long)g - color[c].g;
            long db = (long)b - color[c].b;
            long dist = dr * dr + dg * dg + db * db;
            if (bestDist < 0 || dist < bestDist) {
                bestDist = dist;
                best = c;
            }
        }
        lut[i] = (LayerBit)best;
    }
    lut[0] = 0; /* transparent key */

    rowBytes = ((width * 4u) + 31u) / 32u * 4u;
    *pixels = (LayerBit *)alloc_large((size_t)width * height);
    *w = (int)width;
    *h = (int)height;
    if (compat_file_seek(file, (long)dataOffset, 0) != 0) {
        compat_file_close(file);
        return -1;
    }
    for (y = 0; y < height; y++) {
        LayerBit *dst = *pixels + (size_t)(height - 1u - y) * width;
        uint8_t row[128];
        uint32_t x;
        if (rowBytes > sizeof(row)) {
            compat_file_close(file);
            return -1;
        }
        if (compat_file_read(row, 1, rowBytes, file) != rowBytes) {
            compat_file_close(file);
            return -1;
        }
        for (x = 0; x < width; x++) {
            const uint8_t packed = row[x >> 1];
            const uint8_t nibble = (x & 1u) ? (packed & 0x0fu)
                                            : (packed >> 4);
            dst[x] = lut[nibble & 0x0fu];
        }
    }
    compat_file_close(file);
    return 0;
}

static void loadSprites(void)
{
    int i;
    for (i = 0; i < SPRITE_NUM; i++) {
        sprite[i] = NULL;
        spriteW[i] = 0;
        spriteH[i] = 0;
        if (loadBmp4(spriteFile[i], &sprite[i], &spriteW[i],
                     &spriteH[i]) != 0) {
            exit(1);
        }
    }
}

void drawSprite(int n, int x, int y)
{
    int sx, sy;
    if (n < 0 || n >= SPRITE_NUM || sprite[n] == NULL) {
        return;
    }
    for (sy = 0; sy < spriteH[n]; sy++) {
        const int vy = y + sy;
        if (vy < 0 || vy >= VIDEO_H) {
            continue;
        }
        for (sx = 0; sx < spriteW[n]; sx++) {
            const LayerBit value = sprite[n][sx + sy * spriteW[n]];
            const int vx = x + sx;
            if (value == 0 || vx < 0 || vx >= VIDEO_W) {
                continue;
            }
            VIDEO_AT(vx, vy) = value;
        }
    }
}

void initSDL(void)
{
    if (compat_init() != 0) {
        fprintf(stderr, "noiz2sa: compat_init failed\n");
        exit(1);
    }
    printf("noiz2sa: framebuffer %ux%u\n",
           compat_framebuffer()->width, compat_framebuffer()->height);

    video = (LayerBit *)alloc_large((size_t)VIDEO_W * VIDEO_H);
    buf = (LayerBit *)alloc_large((size_t)LAYER_WIDTH * LAYER_HEIGHT);
    lpbuf = (LayerBit *)alloc_large((size_t)PANEL_WIDTH * PANEL_HEIGHT);
    rpbuf = (LayerBit *)alloc_large((size_t)PANEL_WIDTH * PANEL_HEIGHT);

    /* The engine expects two full-size composition layers plus the smoke
       source; all three are 8bpp index buffers. */
    l1buf = (LayerBit *)alloc_large((size_t)LAYER_WIDTH * LAYER_HEIGHT);
    l2buf = (LayerBit *)alloc_large((size_t)LAYER_WIDTH * LAYER_HEIGHT);
    pbuf = (LayerBit *)alloc_large((size_t)LAYER_WIDTH * LAYER_HEIGHT);

    initPalette();
    makeSmokeBuf();
    clearLPanel();
    clearRPanel();

    keys = noiz2sa_port_keyboard_state();
    loadSprites();
}

void closeSDL(void)
{
    /* Buffers stay allocated for the process lifetime; the engine exits
       immediately after closeSDL(). */
}

void blendScreen(void)
{
    int i;
    for (i = lyrSize - 1; i >= 0; i--) {
        buf[i] = colorAlp[l1buf[i]][l2buf[i]];
    }
}

void clearScreen(void)
{
    memset(buf, 0, (size_t)LAYER_WIDTH * LAYER_HEIGHT);
}

void clearLPanel(void)
{
    memset(lpbuf, 0, (size_t)PANEL_WIDTH * PANEL_HEIGHT);
}

void clearRPanel(void)
{
    memset(rpbuf, 0, (size_t)PANEL_WIDTH * PANEL_HEIGHT);
}

void smokeScreen(void)
{
    int i;
    memcpy(pbuf, l2buf, (size_t)lyrSize);
    for (i = lyrSize - 1; i >= 0; i--) {
        const int32_t ofs = smokeOfs[i];
        l1buf[i] = colorDfs[l1buf[i]];
        l2buf[i] = colorDfs[ofs < 0 ? 0 : pbuf[ofs]];
    }
}

/* 2:1 downscale operators for the S17-F hardware comparison are selected
   through noiz2sa_port_set_downscale_mode(). */
void noiz2sa_port_set_downscale_mode(int mode)
{
    downscaleMode = mode;
}

static void composeVideo(void)
{
    int y;
    for (y = 0; y < VIDEO_H; y++) {
        LayerBit *dst = video + (size_t)y * VIDEO_W;
        memcpy(dst + LPANEL_X, lpbuf + (size_t)y * PANEL_WIDTH,
               PANEL_WIDTH);
        memcpy(dst + LAYER_X, buf + (size_t)y * LAYER_WIDTH, LAYER_WIDTH);
        memcpy(dst + RPANEL_X, rpbuf + (size_t)y * PANEL_WIDTH,
               PANEL_WIDTH);
    }
}

static void presentDownscaled(void)
{
    compat_framebuffer_t *fb = compat_framebuffer();
    unsigned oy, ox;
    for (oy = 0; oy < COMPAT_SCREEN_HEIGHT; oy++) {
        compat_pixel_t *dst = fb->pixels + (size_t)oy * fb->stride;
        const LayerBit *row0 = video + (size_t)(oy * 2) * VIDEO_W;
        const LayerBit *row1 = row0 + VIDEO_W;
        for (ox = 0; ox < COMPAT_SCREEN_WIDTH; ox++) {
            const int x = (int)ox * 2;
            const LayerBit a = row0[x];
            const LayerBit b = row0[x + 1];
            const LayerBit c = row1[x];
            const LayerBit d = row1[x + 1];
            if (downscaleMode == NOIZ2SA_SCALE_AVERAGE) {
                unsigned r = 0, g = 0, bl = 0;
                const LayerBit idx[4] = {a, b, c, d};
                int k;
                for (k = 0; k < 4; k++) {
                    const compat_pixel_t p = palette565[idx[k]];
                    r += (p >> 11) & 0x1fu;
                    g += (p >> 5) & 0x3fu;
                    bl += p & 0x1fu;
                }
                dst[ox] = (compat_pixel_t)(((r / 4u) << 11) |
                                           ((g / 4u) << 5) | (bl / 4u));
            } else {
                LayerBit best = a;
                if (paletteLuma[b] > paletteLuma[best]) {
                    best = b;
                }
                if (paletteLuma[c] > paletteLuma[best]) {
                    best = c;
                }
                if (paletteLuma[d] > paletteLuma[best]) {
                    best = d;
                }
                dst[ox] = palette565[best];
            }
        }
    }
}

void flipScreen(void)
{
    composeVideo();
    if (status == TITLE) {
        drawTitle();
    }
    presentDownscaled();
    compat_present();
}

/* ------------------------------------------------------------------ */
/* Drawing primitives: upstream buffer algorithms, unchanged.          */
/* ------------------------------------------------------------------ */

void drawLine(int x1, int y1, int x2, int y2, LayerBit color, int width,
              LayerBit *buf)
{
    int lx, ly, ax, ay, x, y, ptr, i, j;
    int xMax, yMax;

    lx = absN(x2 - x1);
    ly = absN(y2 - y1);
    if (lx < ly) {
        x1 -= width >> 1;
        x2 -= width >> 1;
    } else {
        y1 -= width >> 1;
        y2 -= width >> 1;
    }
    xMax = LAYER_WIDTH - width - 1;
    yMax = LAYER_HEIGHT - width - 1;

    if (x1 < 0) {
        if (x2 < 0) return;
        y1 = (y1 - y2) * x2 / (x2 - x1) + y2;
        x1 = 0;
    } else if (x2 < 0) {
        y2 = (y2 - y1) * x1 / (x1 - x2) + y1;
        x2 = 0;
    }
    if (x1 > xMax) {
        if (x2 > xMax) return;
        y1 = (y1 - y2) * (x2 - xMax) / (x2 - x1) + y2;
        x1 = xMax;
    } else if (x2 > xMax) {
        y2 = (y2 - y1) * (x1 - xMax) / (x1 - x2) + y1;
        x2 = xMax;
    }
    if (y1 < 0) {
        if (y2 < 0) return;
        x1 = (x1 - x2) * y2 / (y2 - y1) + x2;
        y1 = 0;
    } else if (y2 < 0) {
        x2 = (x2 - x1) * y1 / (y1 - y2) + x1;
        y2 = 0;
    }
    if (y1 > yMax) {
        if (y2 > yMax) return;
        x1 = (x1 - x2) * (y2 - yMax) / (y2 - y1) + x2;
        y1 = yMax;
    } else if (y2 > yMax) {
        x2 = (x2 - x1) * (y1 - yMax) / (y1 - y2) + x1;
        y2 = yMax;
    }

    lx = abs(x2 - x1);
    ly = abs(y2 - y1);

    if (lx < ly) {
        if (ly == 0) ly++;
        ax = ((x2 - x1) << 8) / ly;
        ay = ((y2 - y1) >> 8) | 1;
        x = x1 << 8;
        y = y1;
        for (i = ly; i > 0; i--, x += ax, y += ay) {
            ptr = y * pitch + (x >> 8);
            for (j = width; j > 0; j--, ptr++) {
                buf[ptr] = color;
            }
        }
    } else {
        if (lx == 0) lx++;
        ay = ((y2 - y1) << 8) / lx;
        ax = ((x2 - x1) >> 8) | 1;
        x = x1;
        y = y1 << 8;
        for (i = lx; i > 0; i--, x += ax, y += ay) {
            ptr = (y >> 8) * pitch + x;
            for (j = width; j > 0; j--, ptr += pitch) {
                buf[ptr] = color;
            }
        }
    }
}

void drawThickLine(int x1, int y1, int x2, int y2, LayerBit color1,
                   LayerBit color2, int width)
{
    int lx, ly, ax, ay, x, y, ptr, i, j;
    int xMax, yMax;
    int width1;

    lx = abs(x2 - x1);
    ly = abs(y2 - y1);
    if (lx < ly) {
        x1 -= width >> 1;
        x2 -= width >> 1;
    } else {
        y1 -= width >> 1;
        y2 -= width >> 1;
    }
    xMax = LAYER_WIDTH - width;
    yMax = LAYER_HEIGHT - width;

    if (x1 < 0) {
        if (x2 < 0) return;
        y1 = (y1 - y2) * x2 / (x2 - x1) + y2;
        x1 = 0;
    } else if (x2 < 0) {
        y2 = (y2 - y1) * x1 / (x1 - x2) + y1;
        x2 = 0;
    }
    if (x1 > xMax) {
        if (x2 > xMax) return;
        y1 = (y1 - y2) * (x2 - xMax) / (x2 - x1) + y2;
        x1 = xMax;
    } else if (x2 > xMax) {
        y2 = (y2 - y1) * (x1 - xMax) / (x1 - x2) + y1;
        x2 = xMax;
    }
    if (y1 < 0) {
        if (y2 < 0) return;
        x1 = (x1 - x2) * y2 / (y2 - y1) + x2;
        y1 = 0;
    } else if (y2 < 0) {
        x2 = (x2 - x1) * y1 / (y1 - y2) + x1;
        y2 = 0;
    }
    if (y1 > yMax) {
        if (y2 > yMax) return;
        x1 = (x1 - x2) * (y2 - yMax) / (y2 - y1) + x2;
        y1 = yMax;
    } else if (y2 > yMax) {
        x2 = (x2 - x1) * (y1 - yMax) / (y1 - y2) + x1;
        y2 = yMax;
    }

    lx = abs(x2 - x1);
    ly = abs(y2 - y1);
    width1 = width - 2;

    if (lx < ly) {
        if (ly == 0) ly++;
        ax = ((x2 - x1) << 8) / ly;
        ay = ((y2 - y1) >> 8) | 1;
        x = x1 << 8;
        y = y1;
        ptr = y * pitch + (x >> 8) + 1;
        memset(&(buf[ptr]), color2, width1);
        x += ax;
        y += ay;
        for (i = ly - 1; i > 1; i--, x += ax, y += ay) {
            ptr = y * pitch + (x >> 8);
            buf[ptr] = color2;
            ptr++;
            memset(&(buf[ptr]), color1, width1);
            ptr += width1;
            buf[ptr] = color2;
        }
        ptr = y * pitch + (x >> 8) + 1;
        memset(&(buf[ptr]), color2, width1);
    } else {
        if (lx == 0) lx++;
        ay = ((y2 - y1) << 8) / lx;
        ax = ((x2 - x1) >> 8) | 1;
        x = x1;
        y = y1 << 8;
        ptr = ((y >> 8) + 1) * pitch + x;
        for (j = width1; j > 0; j--, ptr += pitch) {
            buf[ptr] = color2;
        }
        x += ax;
        y += ay;
        for (i = lx - 1; i > 1; i--, x += ax, y += ay) {
            ptr = (y >> 8) * pitch + x;
            buf[ptr] = color2;
            ptr += pitch;
            for (j = width1; j > 0; j--, ptr += pitch) {
                buf[ptr] = color1;
            }
            buf[ptr] = color2;
        }
        ptr = ((y >> 8) + 1) * pitch + x;
        for (j = width1; j > 0; j--, ptr += pitch) {
            buf[ptr] = color2;
        }
    }
}

void drawBox(int x, int y, int width, int height, LayerBit color1,
             LayerBit color2, LayerBit *buf)
{
    int i;
    int ptr;

    x -= width >> 1;
    y -= height >> 1;
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (x + width >= LAYER_WIDTH) {
        width = LAYER_WIDTH - x;
    }
    if (width <= 1) return;
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (y + height > LAYER_HEIGHT) {
        height = LAYER_HEIGHT - y;
    }
    if (height <= 1) return;

    ptr = x + y * LAYER_WIDTH;
    memset(&(buf[ptr]), color2, width);
    y++;
    for (i = 0; i < height - 2; i++, y++) {
        ptr = x + y * LAYER_WIDTH;
        buf[ptr] = color2;
        ptr++;
        memset(&(buf[ptr]), color1, width - 2);
        ptr += width - 2;
        buf[ptr] = color2;
    }
    ptr = x + y * LAYER_WIDTH;
    memset(&(buf[ptr]), color2, width);
}

void drawBoxPanel(int x, int y, int width, int height, LayerBit color1,
                  LayerBit color2, LayerBit *buf)
{
    int i;
    int ptr;

    x -= width >> 1;
    y -= height >> 1;
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (x + width >= PANEL_WIDTH) {
        width = PANEL_WIDTH - x;
    }
    if (width <= 1) return;
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (y + height > PANEL_HEIGHT) {
        height = PANEL_HEIGHT - y;
    }
    if (height <= 1) return;

    ptr = x + y * PANEL_WIDTH;
    memset(&(buf[ptr]), color2, width);
    y++;
    for (i = 0; i < height - 2; i++, y++) {
        ptr = x + y * PANEL_WIDTH;
        buf[ptr] = color2;
        ptr++;
        memset(&(buf[ptr]), color1, width - 2);
        ptr += width - 2;
        buf[ptr] = color2;
    }
    ptr = x + y * PANEL_WIDTH;
    memset(&(buf[ptr]), color2, width);
}

int drawNum(int n, int x, int y, int s, int c1, int c2)
{
    for (;;) {
        drawLetter(n % 10, x, y, s, 1, c1, c2, lpbuf);
        y += s * 1.7f;
        n /= 10;
        if (n <= 0) break;
    }
    return y;
}

int drawNumRight(int n, int x, int y, int s, int c1, int c2)
{
    int d, nd, drawn = 0;
    for (d = 100000000; d > 0; d /= 10) {
        nd = (int)(n / d);
        if (nd > 0 || drawn) {
            n -= d * nd;
            drawLetter(nd % 10, x, y, s, 3, c1, c2, rpbuf);
            y += s * 1.7f;
            drawn = 1;
        }
    }
    if (!drawn) {
        drawLetter(0, x, y, s, 3, c1, c2, rpbuf);
        y += s * 1.7f;
    }
    return y;
}

int drawNumCenter(int n, int x, int y, int s, int c1, int c2)
{
    for (;;) {
        drawLetterBuf(n % 10, x, y, s, 2, c1, c2, buf, 0);
        x -= s * 1.7f;
        n /= 10;
        if (n <= 0) break;
    }
    return y;
}

/* ------------------------------------------------------------------ */
/* Input: compat v1 keys -> engine PAD_* bits, plus the scripted        */
/* input hook used by the deterministic replay tests.                  */
/* ------------------------------------------------------------------ */

static int latchedFrame = -1;
static int latchedPad;
static int latchedButtons;

static void latchInput(void)
{
    const int frame = noiz2sa_port_frame();
    int pad = 0;
    int buttons = 0;
    noiz2sa_port_input_hook_t hook;

    if (frame == latchedFrame) {
        return;
    }
    latchedFrame = frame;

    if (compat_key_down(COMPAT_KEY_LEFT)) {
        pad |= PAD_LEFT;
    }
    if (compat_key_down(COMPAT_KEY_RIGHT)) {
        pad |= PAD_RIGHT;
    }
    if (compat_key_down(COMPAT_KEY_UP)) {
        pad |= PAD_UP;
    }
    if (compat_key_down(COMPAT_KEY_DOWN)) {
        pad |= PAD_DOWN;
    }
    if (compat_key_down(COMPAT_KEY_JUMP)) {
        buttons |= buttonReversed ? PAD_BUTTON2 : PAD_BUTTON1;
    }
    if (compat_key_down(COMPAT_KEY_ACTION)) {
        buttons |= buttonReversed ? PAD_BUTTON1 : PAD_BUTTON2;
    }

    hook = noiz2sa_port_input_hook_target();
    if (hook != NULL) {
        hook(frame, &pad, &buttons);
    }
    latchedPad = pad;
    latchedButtons = buttons;
}

int getPadState(void)
{
    latchInput();
    return latchedPad;
}

int getButtonState(void)
{
    latchInput();
    return latchedButtons;
}





