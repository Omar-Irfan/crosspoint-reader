#pragma once

#include "activities/Activity.h"
#include "util/ReadingLog.h"

// One year of reading as a GitHub-style grid with monthly totals. Earlier and Later
// move between years (the front Left/Right buttons the hints sit over, or the
// side buttons); Back returns to the stats page.
class ReadingYearActivity final : public Activity {
  readinglog::YearSummary year;

  void loadYear(int yearNumber);

 public:
  explicit ReadingYearActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadingYear", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
