/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "perf_report_c.h"

#include "perf_report.h"

namespace {
RetroPerfReport reporter;
}

extern "C" void retropicoes_perf_begin(const char *name, uint64_t now,
                                         uint32_t budget_us,
                                         uint32_t target_fps10)
{
    reporter.begin(name, now, budget_us, target_fps10);
}

extern "C" void retropicoes_perf_frame(
    const retropicoes_perf_frame_t *frame, uint64_t now)
{
    if (frame != nullptr) reporter.frame(*frame, now);
}

extern "C" void retropicoes_perf_pump(void)
{
    reporter.pump();
}
