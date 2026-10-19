/*
 * Minimal host-only PNG writer for S17 verification.
 *
 * Writes an uncompressed (stored-deflate) RGB888 PNG so the ported video
 * pipeline output can be inspected without any third-party library. Used by
 * the host replay runner when a dump path is given; never linked into a
 * target build.
 */
#include "compat.h"
#include "noiz2sa_port_testhooks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long crcTable[256];
static int crcReady;

static void crcInit(void)
{
    unsigned long c;
    int n, k;
    for (n = 0; n < 256; n++) {
        c = (unsigned long)n;
        for (k = 0; k < 8; k++) {
            c = (c & 1) ? (0xedb88320UL ^ (c >> 1)) : (c >> 1);
        }
        crcTable[n] = c;
    }
    crcReady = 1;
}

static unsigned long crcUpdate(unsigned long crc, const unsigned char *buf,
                               size_t len)
{
    size_t n;
    for (n = 0; n < len; n++) {
        crc = crcTable[(crc ^ buf[n]) & 0xff] ^ (crc >> 8);
    }
    return crc;
}

static void put32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static int writeChunk(FILE *f, const char *type, const unsigned char *data,
                      size_t len)
{
    unsigned char hdr[8];
    unsigned char *crcBuf;
    unsigned long crc = 0xffffffffUL;
    put32(hdr, (unsigned long)len);
    memcpy(hdr + 4, type, 4);
    if (fwrite(hdr, 1, 8, f) != 8) {
        return -1;
    }
    if (len > 0 && fwrite(data, 1, len, f) != len) {
        return -1;
    }
    crcBuf = (unsigned char *)malloc(4 + len);
    if (crcBuf == NULL) {
        return -1;
    }
    memcpy(crcBuf, type, 4);
    if (len > 0) {
        memcpy(crcBuf + 4, data, len);
    }
    crc = crcUpdate(crc, crcBuf, 4 + len) ^ 0xffffffffUL;
    free(crcBuf);
    put32(hdr, crc);
    return fwrite(hdr, 1, 4, f) == 4 ? 0 : -1;
}

int noiz2sa_port_dump_frame_png(const char *path)
{
    compat_framebuffer_t *fb = compat_framebuffer();
    const unsigned w = fb->width;
    const unsigned h = fb->height;
    const size_t rawStride = (size_t)w * 3u + 1u;
    const size_t rawSize = rawStride * h;
    unsigned char *raw;
    unsigned char ihdr[13];
    FILE *f;
    unsigned x, y;

    if (!crcReady) {
        crcInit();
    }
    raw = (unsigned char *)malloc(rawSize);
    if (raw == NULL) {
        return -1;
    }
    for (y = 0; y < h; y++) {
        const compat_pixel_t *row = fb->pixels + (size_t)y * fb->stride;
        unsigned char *dst = raw + (size_t)y * rawStride;
        *dst++ = 0; /* filter: none */
        for (x = 0; x < w; x++) {
            const compat_pixel_t p = row[x];
            const unsigned r = (p >> 11) & 0x1fu;
            const unsigned g = (p >> 5) & 0x3fu;
            const unsigned b = p & 0x1fu;
            *dst++ = (unsigned char)((r << 3) | (r >> 2));
            *dst++ = (unsigned char)((g << 2) | (g >> 4));
            *dst++ = (unsigned char)((b << 3) | (b >> 2));
        }
    }

    f = fopen(path, "wb");
    if (f == NULL) {
        free(raw);
        return -1;
    }
    {
        static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
        if (fwrite(sig, 1, 8, f) != 8) {
            fclose(f);
            free(raw);
            return -1;
        }
    }
    put32(ihdr, w);
    put32(ihdr + 4, h);
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 2;  /* color type: truecolor */
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    if (writeChunk(f, "IHDR", ihdr, sizeof(ihdr)) != 0) {
        fclose(f);
        free(raw);
        return -1;
    }
    /* Single zlib stream built from stored (uncompressed) deflate blocks. */
    {
        const size_t maxBlock = 65535u;
        size_t offset = 0;
        unsigned char zlibHdr[2] = {0x78, 0x01};
        unsigned long a = 1, b = 0;
        if (fwrite(zlibHdr, 1, 2, f) != 2) {
            fclose(f);
            free(raw);
            return -1;
        }
        while (offset < rawSize) {
            const size_t n = (rawSize - offset) > maxBlock
                                 ? maxBlock
                                 : (rawSize - offset);
            unsigned char bh[5];
            const int final = (offset + n) >= rawSize;
            bh[0] = (unsigned char)(final ? 1 : 0);
            bh[1] = (unsigned char)(n & 0xffu);
            bh[2] = (unsigned char)((n >> 8) & 0xffu);
            bh[3] = (unsigned char)(~n & 0xffu);
            bh[4] = (unsigned char)((~n >> 8) & 0xffu);
            if (fwrite(bh, 1, 5, f) != 5 ||
                fwrite(raw + offset, 1, n, f) != n) {
                fclose(f);
                free(raw);
                return -1;
            }
            offset += n;
        }
        for (offset = 0; offset < rawSize; offset++) {
            a = (a + raw[offset]) % 65521u;
            b = (b + a) % 65521u;
        }
        {
            unsigned char adler[4];
            put32(adler, (b << 16) | a);
            if (fwrite(adler, 1, 4, f) != 4) {
                fclose(f);
                free(raw);
                return -1;
            }
        }
    }
    if (writeChunk(f, "IEND", NULL, 0) != 0) {
        fclose(f);
        free(raw);
        return -1;
    }
    fclose(f);
    free(raw);
    return 0;
}
