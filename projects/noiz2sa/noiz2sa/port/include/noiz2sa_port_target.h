/*
 * Target-only adaptation surface for the noiz2sa port.
 *
 * These entry points exist for the RP2350B build (S17-D) and the S17-F
 * hardware comparison. They are declared separately from the host test hooks
 * so the host runner cannot accidentally depend on target policy, and so a
 * target backend knows exactly what it is expected to provide.
 */
#ifndef NOIZ2SA_PORT_TARGET_H
#define NOIZ2SA_PORT_TARGET_H

#include "noiz2sa_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Video: 2:1 downscale operator selection (0 = brightest, 1 = average),
   used by the S17-F hardware comparison. Implemented in port/src/screen.c. */
#define NOIZ2SA_DOWNSCALE_BRIGHTEST 0
#define NOIZ2SA_DOWNSCALE_AVERAGE 1
void noiz2sa_port_set_downscale_mode(int mode);

/* Pico target bootstrap; implemented by the private target adapter. */
int noiz2sa_port_target_run(void);

/*
 * Path mapping for the FatFS-backed target: engine-relative names such as
 * "noiz2sa.prf", "zako" and "images/title_a.bmp" are resolved against
 * /SAVES/noiz2sa/ (preferences) or /data/noiz2sa/ (read-only data).
 * The host build returns the path unchanged.
 *
 * S17-D placeholder: declared here, not yet implemented by any backend and
 * not yet called by the engine.
 */
const char *noiz2sa_port_map_path(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* NOIZ2SA_PORT_TARGET_H */
