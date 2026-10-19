/* S17-B memory layout evidence for the noiz2sa RetroPicoES port.
 *
 * Prints and statically checks the sizes of the engine's hot and cold
 * objects using only upstream headers. The numbers feed the PSRAM/SRAM
 * budget documented in doc/RETROPICOES_V21_S17B_MEMORY.md.
 */
/* Engine C headers declare functions outside their include guards, so in C++
 * they must all be seen with C linkage exactly once. foe.h is C++-only. */
extern "C" {
#include "ship.h"
#include "shot.h"
#include "noiz2sa.h"
#include "screen.h"
}
#include "foe.h"

#include <cstdio>
#include <cstdint>

/* Layer buffer bytes: LayerBit=uint8, LAYER 320x480, panels 160x480. */
static const size_t kLayerBytes = (size_t)LAYER_WIDTH * LAYER_HEIGHT;
static const size_t kPanelBytes = (size_t)PANEL_WIDTH * PANEL_HEIGHT;

/* Mirrors private constants of foe.cc; keep in sync with the engine. */
static const int kFoeMax = 1024;

int main()
{
    /* Hot SRAM candidates. */
    const size_t foePool = (size_t)kFoeMax * sizeof(Foe);
    const size_t shotPool = (size_t)SHOT_MAX * sizeof(Shot);
    printf("foe pool:      %zu B (FOE_MAX=%d sizeof(Foe)=%zu)\n",
           foePool, kFoeMax, sizeof(Foe));
    printf("shot pool:     %zu B (SHOT_MAX=%d sizeof(Shot)=%zu)\n",
           shotPool, SHOT_MAX, sizeof(Shot));
    printf("ship:          %zu B\n", sizeof(Ship));
    printf("palette:       %u B (256 x RGB565)\n", 256u * 2u);

    /* Cold PSRAM candidates: six 8bpp index layers
       (l1, l2, pbuf plus two 160x480 panels; smoke needs one more). */
    const size_t layers = kLayerBytes * 4u + kPanelBytes * 2u;
    printf("index layers:  %zu B (l1/l2/pbuf=%zu B each, panels=%zu B each)\n",
           layers, kLayerBytes, kPanelBytes);
    printf("rgb565 mid:    %zu B (640x480 mid buffer before downscale)\n",
           (size_t)640u * 480u * 2u);

    /* Budget guards (doc/RETROPICOES_V21_S17B_MEMORY.md). */
    if (foePool > 128u * 1024u) { fprintf(stderr, "foe pool over SRAM budget\n"); return 1; }
    if (shotPool > 1024u) { fprintf(stderr, "shot pool over budget\n"); return 1; }
    if (layers > 1024u * 1024u) { fprintf(stderr, "index layers over PSRAM budget\n"); return 1; }

    printf("NOIZ2SA MEMINFO: PASS\n");
    return 0;
}
