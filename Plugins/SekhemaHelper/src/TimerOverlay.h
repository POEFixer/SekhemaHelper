#pragma once
// TimerOverlay.h — compact run / floor / room timer window (default on,
// draggable, theme-aware; visible only in-trial + a short post-run linger).
#include "RunTracker.h"
#include "Settings.h"

namespace sekhema {

void DrawTimerOverlay(const RunTracker::Overlay& ov, Settings& s, uint64_t nowMs);

} // namespace sekhema
