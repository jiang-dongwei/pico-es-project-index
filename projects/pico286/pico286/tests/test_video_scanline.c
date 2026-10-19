#include "video_scanline.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static void fill_palette(uint16_t *palette, size_t count)
{
    for (size_t index = 0u; index < count; ++index) {
        palette[index] = (uint16_t)(index * 17u);
    }
}

int main(void)
{
    uint16_t palette[256];
    uint16_t guarded[322] = {0};
    uint8_t indexed160[160];
    uint8_t indexed640[640];
    uint8_t packed4[] = {0x12u, 0xFEu};
    uint8_t packed2[] = {0x1Bu};
    uint8_t packed1[] = {0xA5u};
    uint8_t text_cells[80u * 25u * 2u] = {0};
    uint8_t font[256u * 16u] = {0};
    pico286_scanline_source_t source = {0};
    pico286_text_scanline_source_t text_source = {0};

    fill_palette(palette, 256u);
    for (size_t index = 0u; index < 160u; ++index) {
        indexed160[index] = (uint8_t)index;
    }
    for (size_t index = 0u; index < 640u; ++index) {
        indexed640[index] = (uint8_t)index;
    }

    guarded[0] = 0x1234u;
    guarded[321] = 0x5678u;
    source = (pico286_scanline_source_t){
        indexed160, sizeof(indexed160), 160u,
        PICO286_SCANLINE_INDEX8, palette, 256u};
    assert(pico286_convert_scanline_rgb565(&source, &guarded[1], 320u) ==
           PICO286_SCANLINE_OK);
    assert(guarded[1] == palette[0]);
    assert(guarded[2] == palette[0]);
    assert(guarded[319] == palette[159]);
    assert(guarded[320] == palette[159]);
    assert(guarded[0] == 0x1234u && guarded[321] == 0x5678u);

    source.pixels = indexed640;
    source.byte_count = sizeof(indexed640);
    source.width = 640u;
    assert(pico286_convert_scanline_rgb565(&source, &guarded[1], 320u) ==
           PICO286_SCANLINE_OK);
    assert(guarded[1] == palette[0]);
    assert(guarded[2] == palette[2]);
    assert(guarded[320] == palette[(uint8_t)638u]);

    source = (pico286_scanline_source_t){
        packed4, sizeof(packed4), 4u,
        PICO286_SCANLINE_PACKED_4BPP, palette, 16u};
    assert(pico286_convert_scanline_rgb565(&source, &guarded[1], 4u) ==
           PICO286_SCANLINE_OK);
    assert(guarded[1] == palette[1] && guarded[2] == palette[2]);
    assert(guarded[3] == palette[15] && guarded[4] == palette[14]);

    source = (pico286_scanline_source_t){
        packed2, sizeof(packed2), 4u,
        PICO286_SCANLINE_PACKED_2BPP, palette, 4u};
    assert(pico286_convert_scanline_rgb565(&source, &guarded[1], 4u) ==
           PICO286_SCANLINE_OK);
    assert(guarded[1] == palette[0] && guarded[2] == palette[1]);
    assert(guarded[3] == palette[2] && guarded[4] == palette[3]);

    source = (pico286_scanline_source_t){
        packed1, sizeof(packed1), 8u,
        PICO286_SCANLINE_PACKED_1BPP, palette, 2u};
    assert(pico286_convert_scanline_rgb565(&source, &guarded[1], 8u) ==
           PICO286_SCANLINE_OK);
    assert(guarded[1] == palette[1] && guarded[2] == palette[0]);
    assert(guarded[7] == palette[0] && guarded[8] == palette[1]);

    source.byte_count = 0u;
    assert(pico286_convert_scanline_rgb565(&source, &guarded[1], 8u) ==
           PICO286_SCANLINE_SOURCE_SHORT);
    source.byte_count = sizeof(packed1);
    source.palette_entries = 1u;
    assert(pico286_convert_scanline_rgb565(&source, &guarded[1], 8u) ==
           PICO286_SCANLINE_PALETTE_SHORT);
    source.format = (pico286_scanline_format_t)99;
    assert(pico286_convert_scanline_rgb565(&source, &guarded[1], 8u) ==
           PICO286_SCANLINE_BAD_FORMAT);
    assert(pico286_convert_scanline_rgb565(NULL, &guarded[1], 8u) ==
           PICO286_SCANLINE_NULL_ARGUMENT);

    text_cells[0] = 65u;
    text_cells[1] = 0x1Eu;
    font[65u * 16u] = 0xCCu;
    text_source = (pico286_text_scanline_source_t){
        text_cells, sizeof(text_cells), font, sizeof(font),
        palette, 16u, 0u, 1u};
    assert(pico286_render_text_scanline_rgb565(
               &text_source, &guarded[1], 320u) == PICO286_SCANLINE_OK);
    assert(guarded[1] == palette[14] && guarded[2] == palette[1]);
    assert(guarded[3] == palette[14] && guarded[4] == palette[1]);

    text_cells[1] = 0x9Eu;
    text_source.blink_visible = 0u;
    assert(pico286_render_text_scanline_rgb565(
               &text_source, &guarded[1], 320u) == PICO286_SCANLINE_OK);
    assert(guarded[1] == palette[1] && guarded[3] == palette[1]);
    text_source.source_line = 400u;
    assert(pico286_render_text_scanline_rgb565(
               &text_source, &guarded[1], 320u) ==
           PICO286_SCANLINE_SOURCE_SHORT);

    return 0;
}
