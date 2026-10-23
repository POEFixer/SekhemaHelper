#pragma once
// TimerOverlay.h — compact run / floor / room timer window (default on,
// draggable, theme-aware; visible only in-trial + a short post-run linger).
// An informational window of the host (ctx()->Overlay) with its gear menu; its
// Close clears s.timerOverlayEnabled.
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "RunTracker.h"
#include "Settings.h"

namespace sekhema {

void DrawTimerOverlay(const PluginSDK::OverlayService& overlay, const RunTracker::Overlay& ov,
                      Settings& s, uint64_t nowMs);

} // namespace sekhema
