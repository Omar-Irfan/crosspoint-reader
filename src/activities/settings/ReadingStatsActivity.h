#pragma once

#include "activities/Activity.h"
#include "util/ReadingLog.h"

// Read-only reading habits: hours per week, when you read, and a few plain
// facts. Nothing to set; Back returns to Settings.
class ReadingStatsActivity final : public Activity {
  readinglog::Summary summary;

 public:
  explicit ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadingStats", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
