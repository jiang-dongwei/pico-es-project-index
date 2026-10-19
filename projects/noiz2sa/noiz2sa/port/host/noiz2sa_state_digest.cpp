/* S17-B deterministic-replay state digest for the pinned noiz2sa engine.
 *
 * FNV-1a 64 over engine state reachable via upstream public headers (no
 * upstream source changes). enNum[] is sized by the private FOE_TYPE_MAX in
 * foe.cc (currently 4); the digest mirrors that value.
 */
/*
 * Engine C headers declare functions outside their include guards, so in C++
 * they must all be seen with C linkage exactly once (the engine's own .cc
 * files include them inside extern "C" blocks). foe.h is C++-only and pulls
 * the BulletML headers; it stays outside.
 */
extern "C" {
#include "noiz2sa.h"
#include "attractmanager.h"
#include "ship.h"
#include "shot.h"
#include "brgmng_mtd.h"
#include "bonus.h"
#include "compat.h"
}
#include "foe.h"

#include <cstdio>
#include <cstdint>

static const uint64_t kFnvPrime = 0x100000001b3ULL;
static const uint64_t kFnvBasis = 0xcbf29ce484222325ULL;
static const int kFoeTypeMax = 4;

static void fnv_mix(uint64_t &hash, uint32_t value)
{
    hash ^= value;
    hash *= kFnvPrime;
}

extern "C" void noiz2sa_port_print_state_digest(void)
{
    uint64_t hash = kFnvBasis;
    fnv_mix(hash, (uint32_t)status);
    fnv_mix(hash, (uint32_t)tick);
    fnv_mix(hash, (uint32_t)interval);
    fnv_mix(hash, (uint32_t)score);
    fnv_mix(hash, (uint32_t)left);
    fnv_mix(hash, (uint32_t)stage);
    fnv_mix(hash, (uint32_t)scene);
    fnv_mix(hash, (uint32_t)endless);
    fnv_mix(hash, (uint32_t)insane);
    fnv_mix(hash, (uint32_t)bonusScore);
    fnv_mix(hash, (uint32_t)ship.pos.x);
    fnv_mix(hash, (uint32_t)ship.pos.y);
    fnv_mix(hash, (uint32_t)ship.cnt);
    fnv_mix(hash, (uint32_t)ship.shotCnt);
    fnv_mix(hash, (uint32_t)ship.speed);
    fnv_mix(hash, (uint32_t)ship.invCnt);
    for (int i = 0; i < SHOT_MAX; i++) {
        fnv_mix(hash, (uint32_t)shot[i].pos.x);
        fnv_mix(hash, (uint32_t)shot[i].pos.y);
        fnv_mix(hash, (uint32_t)shot[i].cnt);
    }
    fnv_mix(hash, (uint32_t)foeCnt);
    for (int i = 0; i < kFoeTypeMax; i++) {
        fnv_mix(hash, (uint32_t)enNum[i]);
    }
    std::printf("NOIZ2SA STATE DIGEST %016llx tick=%d status=%d score=%d\n",
                (unsigned long long)hash, tick, status, score);
}

/* Digest of the presented RGB565 framebuffer: proves the compose ->
   palette -> 2:1 downscale path ran and produced a replay-identical image.
   nonzero counts non-black pixels so an all-black frame cannot pass. */
extern "C" void noiz2sa_port_print_frame_hash(void)
{
    compat_framebuffer_t *fb = compat_framebuffer();
    uint64_t hash = kFnvBasis;
    unsigned nonZero = 0;
    unsigned x, y;

    for (y = 0; y < fb->height; y++) {
        const compat_pixel_t *row = fb->pixels + (size_t)y * fb->stride;
        for (x = 0; x < fb->width; x++) {
            fnv_mix(hash, (uint32_t)row[x]);
            if (row[x] != 0) {
                nonZero++;
            }
        }
    }
    std::printf("NOIZ2SA FRAME HASH %016llx nonzero=%u\n",
                (unsigned long long)hash, nonZero);
}

