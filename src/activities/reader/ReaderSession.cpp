#include "ReaderSession.h"

#include <algorithm>

namespace {
constexpr uint32_t MIN_DWELL_SECONDS = 2;
constexpr uint32_t MAX_DWELL_SECONDS = 1800;
// A page left open longer than this counts as walking away, not reading.
constexpr uint32_t MAX_TRACKED_DWELL_SECONDS = 300;
}  // namespace

uint16_t ReaderSession::clampProgress(const int progressBp) {
  return static_cast<uint16_t>(std::clamp(progressBp, 0, 10000));
}

void ReaderSession::noteTurn(const bool forward, const bool succeeded) { pendingForward = forward && succeeded; }

void ReaderSession::onRenderComplete(const uint32_t monotonicMs, const int64_t trustedEpoch, const int progressBp) {
  const uint16_t progress = clampProgress(progressBp);
  if (pendingForward && hasAnchor && anchorEpoch > 0) {
    const uint32_t elapsedSeconds = (monotonicMs - anchorMs) / 1000;
    if (elapsedSeconds >= MIN_DWELL_SECONDS) {
      const uint32_t contribution = std::min(elapsedSeconds, MAX_DWELL_SECONDS);
      if (duration == 0) {
        startEpoch = anchorEpoch;
        startProgress = anchorProgress;
        endProgress = anchorProgress;
      }
      duration += contribution;
      trackedSeconds += std::min(elapsedSeconds, MAX_TRACKED_DWELL_SECONDS);
      endProgress = std::max(endProgress, progress);
    }
  }

  anchorMs = monotonicMs;
  anchorEpoch = trustedEpoch;
  anchorProgress = progress;
  hasAnchor = true;
  pendingForward = false;
}

uint32_t ReaderSession::takeTrackedSeconds(const uint32_t nowMs, int64_t& epochOut) {
  // ponytail: a backward turn's page is not credited; count it too if rereading matters.
  if (hasAnchor && anchorEpoch > 0) {
    const uint32_t elapsedSeconds = (nowMs - anchorMs) / 1000;
    if (elapsedSeconds >= MIN_DWELL_SECONDS) trackedSeconds += std::min(elapsedSeconds, MAX_TRACKED_DWELL_SECONDS);
    anchorMs = nowMs;  // a second flush, or the next turn, starts from here
  }
  const uint32_t seconds = trackedSeconds;
  epochOut = anchorEpoch;
  trackedSeconds = 0;
  return seconds;
}

void ReaderSession::reset() {
  anchorMs = 0;
  anchorEpoch = 0;
  startEpoch = 0;
  duration = 0;
  trackedSeconds = 0;
  anchorProgress = 0;
  startProgress = 0;
  endProgress = 0;
  hasAnchor = false;
  pendingForward = false;
}
