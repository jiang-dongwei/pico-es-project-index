/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "studio2.h"
#include <stdio.h>
#include <string.h>

static studio2_t machine, copy, no_input;

static int load_image(const char *path, int bios) {
    uint8_t bytes[2048];
    FILE *file = fopen(path, "rb");
    if (!file)
        return -1;
    size_t size = fread(bytes, 1, sizeof(bytes), file);
    int complete = !ferror(file) && fgetc(file) == EOF;
    fclose(file);
    if (!complete || (!bios && size != 512))
        return -1;
    return bios ? studio2_load_bios(&machine, bytes, size)
                : studio2_load_cart(&machine, bytes, size, 0);
}

static int run(unsigned frames, uint16_t a, uint16_t b, const char *stage) {
    unsigned changes = 0;
    uint8_t previous[128][8];
    machine.keys[0] = copy.keys[0] = a;
    machine.keys[1] = copy.keys[1] = b;
    memcpy(previous, machine.video, sizeof(previous));
    for (unsigned i = 0; i < frames; ++i) {
        studio2_frame(&machine);
        studio2_frame(&copy);
        if (memcmp(&machine, &copy, sizeof(machine)))
            return -1;
        changes += memcmp(previous, machine.video, sizeof(previous)) != 0;
        memcpy(previous, machine.video, sizeof(previous));
    }
    printf("%s: frames=%u video_changes=%u deterministic=PASS\n", stage, frames, changes);
    return (int)changes;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: studio2_tennis_probe BIOS_2048_BYTES TENNIS_512_BYTES\n");
        return 2;
    }
    studio2_init(&machine);
    if (load_image(argv[1], 1) || load_image(argv[2], 0)) {
        fprintf(stderr, "Cannot load BIOS or raw Tennis cartridge\n");
        return 2;
    }
    copy = machine;
    if (run(120, 0, 0, "Boot") < 0 || run(12, 1u << 1, 0, "A1 press") < 0 ||
        run(120, 0, 0, "A1 release") < 0 || run(12, 0, 1u << 4, "B4 press") < 0 ||
        run(120, 0, 0, "B4 release") < 0 || run(12, 1u << 7, 0, "A7 press") < 0)
        return 1;
    if (run(600, 0, 0, "Started game") < 10)
        return 1;
    no_input = machine;
    no_input.keys[0] = no_input.keys[1] = 0;
    for (unsigned i = 0; i < 60; ++i)
        studio2_frame(&no_input);
    if (run(60, 0, 1u << 2, "B2 paddle input") < 1 ||
        !memcmp(machine.video, no_input.video, sizeof(machine.video)) ||
        run(120, 0, 0, "Release") < 0 || !machine.display) {
        fprintf(stderr, "Tennis startup/motion probe failed\n");
        return 1;
    }
    puts("TENNIS STARTUP/MOTION HOST PROBE: PASS (hardware acceptance remains pending)");
    return 0;
}
