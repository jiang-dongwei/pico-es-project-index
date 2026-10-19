/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "trs80.h"
#include "video_adapter.h"
#include <stdio.h>
#include <stdlib.h>
static uint8_t buffer[TRS80_MAX_CMD_BYTES];
static uint8_t disk_buffer[TRS80_MAX_CMD_BYTES];
static int frames(unsigned count)
{
    for (unsigned frame = 0u; frame < count; ++frame)
        if (trs80_run_cycles(trs80_status().clock_hz / 60u) != TRS80_OK) return 0;
    return 1;
}
static int press(unsigned key)
{
    if (!trs80_key(key, 1) || !frames(6u)) return 0;
    trs80_release_keys();
    return frames(30u);
}
int main(int argc, char **argv)
{
    if (argc < 3 || argc > 5) return 2;
    if (trs80_init((trs80_model_t)atoi(argv[1])) != TRS80_OK) return 2;
    FILE *file = fopen(argv[2], "rb");
    if (!file) return 2;
    size_t size = fread(buffer, 1u, sizeof(buffer), file);
    fclose(file);
    if (trs80_load_bios(buffer, size) != TRS80_OK) return 2;
    if (argc == 5) {
        file = fopen(argv[3], "rb");
        if (!file) return 2;
        size = fread(disk_buffer, 1u, sizeof(disk_buffer), file);
        fclose(file);
        trs80_result_t r = trs80_mount_disk(0u, disk_buffer, size,
            (trs80_disk_format_t)atoi(argv[4]));
        if (r != TRS80_OK) { printf("DISK LOAD FAILED: %d\n", r); return 1; }
        trs80_reset();
    }
    if (!frames(120u)) return 1;
    if (argc != 5) {
        if (atoi(argv[1]) >= 3 && !press(12u)) return 1; /* L: low-speed cassette prompt. */
        if (!press(TRS80_MATRIX_ENTER) || !press(TRS80_MATRIX_ENTER)) return 1;
    }
    if (argc == 4) {
        file = fopen(argv[3], "rb");
        if (!file) return 2;
        size = fread(buffer, 1u, sizeof(buffer), file);
        fclose(file);
        trs80_result_t result = trs80_load_cmd(buffer, size);
        if (result != TRS80_OK) { printf("CMD LOAD FAILED: %d\n", result); return 1; }
    }
    for (unsigned frame = 0u; frame < (argc == 5 ? 1800u : 300u); ++frame) {
        if (trs80_run_cycles(trs80_status().clock_hz / 60u) != TRS80_OK) {
            printf("CPU FAULT frame=%u pc=%04x\n", frame, trs80_status().pc);
            return 1;
        }
    }
    trs80_status_t s = trs80_status();
    size_t video_size;
    const uint8_t *video = trs80_video(&video_size);
    if (argc == 5 && !getenv("TRS80_PROBE_DISK_DIAGNOSTIC")) {
        static const char expected[] = "Date";
        int found = 0;
        for (size_t i = 0; i + sizeof(expected) - 1u <= video_size; ++i) {
            unsigned j = 0u;
            while (j < sizeof(expected) - 1u && video[i+j] == (uint8_t)expected[j]) ++j;
            if (j == sizeof(expected) - 1u) found = 1;
        }
        const char *columns = getenv("TRS80_PROBE_BOOT_COLUMNS");
        unsigned expected_columns = columns ? (unsigned)atoi(columns) : (atoi(argv[1]) == 4 ? 80u : 64u);
        if (!found || s.columns != expected_columns ||
            (expected_columns == 80u && (s.rows != 24u || s.memory_map < 2u))) {
            puts("System disk did not reach the expected OS date prompt/mode");
            return 1;
        }
    }
    if (argc == 3) {
        char text[2049];
        for (size_t i = 0u; i < video_size; ++i) {
            unsigned c = video[i];
            text[i] = (char)(c < 32u ? c + 64u : c);
        }
        text[video_size] = '\0';
        int ready = 0;
        for (size_t i = 0u; i + 5u <= video_size; ++i)
            if (text[i] == 'R' && text[i+1] == 'E' && text[i+2] == 'A' &&
                text[i+3] == 'D' && text[i+4] == 'Y') ready = 1;
        if (!ready) { puts("BIOS did not reach BASIC READY"); return 1; }
    }
    if (getenv("TRS80_PROBE_DISK_DIAGNOSTIC")) puts("DIAGNOSTIC ONLY: no system boot assertion");
    printf("CORE PROBE model=%s pc=%04x cycles=%llu cols=%u rows=%u\n",
           argv[1], s.pc, (unsigned long long)s.cycles, s.columns, s.rows);
    const char *frame_path = getenv("TRS80_PROBE_FRAME");
    if (frame_path) {
        static uint16_t pixels[320u*240u];
        if (!trs80_render_video(pixels,320u,320u,240u,(trs80_model_t)atoi(argv[1]),0)) return 1;
        FILE *out = fopen(frame_path,"wb");
        if (!out) return 2;
        fputs("P6\n320 240\n255\n",out);
        for (unsigned i=0;i<320u*240u;++i) {
            uint8_t rgb[3] = {(uint8_t)((pixels[i]>>11)*255u/31u),
                (uint8_t)(((pixels[i]>>5)&63u)*255u/63u),(uint8_t)((pixels[i]&31u)*255u/31u)};
            if (fwrite(rgb,1u,3u,out)!=3u) { fclose(out); return 2; }
        }
        if (fclose(out)) return 2;
    }
    for (unsigned row = 0u; row < s.rows; ++row) {
        for (unsigned col = 0u; col < s.columns; ++col) {
            size_t offset = row * s.columns + col;
            unsigned c = offset < video_size ? video[offset] : 32u;
            if (c < 32u) c += 64u;
            putchar(c >= 32u && c <= 126u ? (int)c : '.');
        }
        putchar('\n');
    }
    return 0;
}
