#pragma once

#include <cstdint>

// Fixed-scalar active-reading summary. The owner reports turn outcomes before
// render and completed pages after render; no allocation or I/O occurs here.
class ReaderSession {
 public:
  void noteTurn(bool forward, bool succeeded);
  void onRenderComplete(uint32_t monotonicMs, int64_t trustedEpoch, int progressBp);
  void reset();
  // Active reading time since the last take, including the page being read now;
  // clears it. A page left open much longer than the reader's recent pages counts
  // as one typical page, and so does the open page at most. The epoch is the last
  // render (0 when the clock was untrusted), the day to credit.
  uint32_t takeTrackedSeconds(uint32_t nowMs, int64_t& epochOut);

  bool isEmitWorthy() const { return startEpoch > 0 && duration >= 10 && endTime() > startEpoch; }
  int64_t startTime() const { return startEpoch; }
  int64_t endTime() const { return startEpoch > 0 ? startEpoch + duration : 0; }
  uint32_t durationSeconds() const { return duration; }
  uint16_t startProgressBp() const { return startProgress; }
  uint16_t endProgressBp() const { return endProgress; }
  // Typical seconds per page, learned from forward turns; 0 until the first. It
  // describes the reader, not one session: reset() keeps it and the owner saves it.
  uint16_t paceSeconds() const { return pace; }
  void restorePace(uint16_t seconds) { pace = seconds; }

 private:
  static uint16_t clampProgress(int progressBp);
  uint32_t creditTurnedPage(uint32_t elapsedSeconds);

  uint32_t anchorMs = 0;
  int64_t anchorEpoch = 0;
  int64_t startEpoch = 0;
  uint32_t duration = 0;
  uint32_t trackedSeconds = 0;
  uint16_t anchorProgress = 0;
  uint16_t startProgress = 0;
  uint16_t endProgress = 0;
  uint16_t pace = 0;
  bool hasAnchor = false;
  bool pendingForward = false;
};
