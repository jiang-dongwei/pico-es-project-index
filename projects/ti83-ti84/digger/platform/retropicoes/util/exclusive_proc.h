#ifndef SDLPOP_RETROPICOES_EXCLUSIVE_PROC_STUB_H
#define SDLPOP_RETROPICOES_EXCLUSIVE_PROC_STUB_H

/*
 * FrensHelpers.cpp includes the PicoDVI exclusive-proc header unconditionally,
 * although all uses are compiled out for the RP2350 HSTX path.  S2-B does not
 * bring PicoDVI into the target, so this intentionally empty header satisfies
 * that unused include without adding the RP2040 video dependency.
 */

#endif
