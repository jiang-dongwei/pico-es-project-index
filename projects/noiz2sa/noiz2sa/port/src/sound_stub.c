/*
 * S17-A headless sound stub for the noiz2sa RetroPicoES port.
 *
 * The pinned soundmanager.c is excluded from the S17-A host build because it
 * depends on SDL_mixer and OGG/WAV file loading. This stub implements the
 * soundmanager.h contract as no-ops so the engine links headlessly. S17-C
 * replaces it with the compat-v1 HDMI audio adapter (WAV effects plus
 * stb_vorbis BGM) without touching the engine.
 */
#include "soundmanager.h"

void closeSound(void)
{
}

void initSound(void)
{
}

void playMusic(int idx)
{
    (void)idx;
}

void fadeMusic(void)
{
}

void stopMusic(void)
{
}

void playChunk(int idx)
{
    (void)idx;
}
