#include "ReaderSession.h"

#include <algorithm>

namespace {
constexpr uint32_t MIN_DWELL_SECONDS = 2;
constexpr uint32_t MAX_DWELL_SECONDS = 1800;
// Reading time follows the reader's own pace: a page that takes more than
// IDLE_FACTOR times the typical page (within the limits below) was left idle.
constexpr uint32_t DEFAULT_PACE_SECONDS = 60;
constexpr uint32_t IDLE_FACTOR = 3;
constexpr uint32_t MIN_IDLE_SECONDS = 120;
constexpr uint32_t MAX_IDLE_SECONDS = 300;
// Shorter pages are flipping to find a place; they do not set the pace.
constexpr uint32_t MIN_PACE_SAMPLE_SECONDS = 10;
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
      trackedSeconds += creditTurnedPage(elapsedSeconds);
      endProgress = std::max(endProgress, progress);
    }
  }

  anchorMs = monotonicMs;
  anchorEpoch = trustedEpoch;
  anchorProgress = progress;
  hasAnchor = true;
  pendingForward = false;
}

uint32_t ReaderSession::creditTurnedPage(const uint32_t elapsedSeconds) {
  const uint32_t typical = pace > 0 ? pace : DEFAULT_PACE_SECONDS;
  const uint32_t idleAfter = std::clamp(typical * IDLE_FACTOR, MIN_IDLE_SECONDS, MAX_IDLE_SECONDS);
  if (elapsedSeconds > idleAfter) {
    // Read it, then walked away. A slow reader's long pages also land here
    // until the pace has grown, so each one moves the pace toward the limit.
    pace = static_cast<uint16_t>((3 * typical + idleAfter + 2) / 4);
    return typical;
  }
  if (elapsedSeconds >= MIN_PACE_SAMPLE_SECONDS) {
    pace = static_cast<uint16_t>((3 * typical + elapsedSeconds + 2) / 4);
  }
  return elapsedSeconds;
}

uint32_t ReaderSession::takeTrackedSeconds(const uint32_t nowMs, int64_t& epochOut) {
  // ponytail: a backward turn's page is not credited; count it too if rereading matters.
  if (hasAnchor && anchorEpoch > 0) {
    // Nothing shows whether the open page was read before the reader slept or
    // closed, so it counts up to one typical page.
    const uint32_t elapsedSeconds = (nowMs - anchorMs) / 1000;
    const uint32_t typical = pace > 0 ? pace : DEFAULT_PACE_SECONDS;
    if (elapsedSeconds >= MIN_DWELL_SECONDS) trackedSeconds += std::min(elapsedSeconds, typical);
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
