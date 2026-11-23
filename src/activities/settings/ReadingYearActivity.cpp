#include "ReadingYearActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <TrustedTime.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "activities/RenderLock.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ReadingActivityView.h"

namespace {
// ponytail: 16 px per day (27 weeks across a half); change with WEEKS_PER_HALF so the grid fits the panel width.
constexpr int CELL = 16;
// ponytail: a year has at most 54 week columns; two stacked halves of 27 keep each cell readable.
constexpr int WEEKS_PER_HALF = 27;
constexpr int HALF_GAP = 36;        // room for the month names above a half
constexpr int GRID_WIDTH = WEEKS_PER_HALF * CELL;
constexpr char MONTH_INITIALS[] = "JFMAMJJASOND";
constexpr const char* MONTH_NAMES[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
}  // namespace

void ReadingYearActivity::loadYear(const int yearNumber) {
  // Load aside, then swap under the lock so a frame in flight never sees a half-filled year.
  readinglog::YearSummary loaded;
  readinglog::loadYear(trustedtime::trustedNow(), yearNumber, loaded);
  {
    RenderLock lock(*this);
    year = loaded;
  }
  requestUpdate();
}

void ReadingYearActivity::onEnter() {
  Activity::onEnter();
  loadYear(0);
}

void ReadingYearActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
  } else if (year.valid && mappedInput.wasPressed(MappedInputManager::Button::NavPrevious) && year.year > year.firstYear) {
    loadYear(year.year - 1);
  } else if (year.valid && mappedInput.wasPressed(MappedInputManager::Button::NavNext) && year.year < year.lastYear) {
    loadYear(year.year + 1);
  }
}

void ReadingYearActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  char title[40];
  snprintf(title, sizeof(title), "%s %d", tr(STR_YEAR), year.year);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, year.valid ? title : tr(STR_YEAR));
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 3;

  if (!year.valid) {
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2 - 10, tr(STR_NO_READING_YET));
  } else {
    // Grid: one column per week, Monday on top, shaded with the sleep screen's levels.
    const int left = (width - GRID_WIDTH) / 2;
    const int halfHeight = 7 * CELL + HALF_GAP;
    y += HALF_GAP - 8;
    const uint8_t jan1Row = readinglog::weekdayOfDay(year.jan1);
    uint32_t monthSeconds[12] = {};
    uint32_t total = 0;
    int daysRead = 0;
    for (uint16_t i = 0; i < year.days; ++i) {
      const uint16_t seconds = year.seconds[i];
      const uint8_t level = readinglog::level(seconds);
      if (level > 0) ++daysRead;
      total += seconds;
      int yearNumber, month, dayOfMonth;
      readinglog::civilFromDay(year.jan1 + i, yearNumber, month, dayOfMonth);
      monthSeconds[month - 1] += seconds;

      const int column = (jan1Row + i) / 7;
      const int row = (jan1Row + i) % 7;
      const int half = column / WEEKS_PER_HALF;
      const int x = left + (column % WEEKS_PER_HALF) * CELL;
      const int cy = y + half * halfHeight + row * CELL;
      if (dayOfMonth == 1) renderer.drawText(SMALL_FONT_ID, x, cy - row * CELL - 22, MONTH_NAMES[month - 1]);
      readingview::drawDayCell(renderer, x, cy, CELL - 1, level);
    }
    y += halfHeight + 7 * CELL - HALF_GAP + metrics.verticalSpacing * 5;

    char line[64];
    char duration[32];
    readingview::formatDuration(total, duration, sizeof(duration));
    snprintf(line, sizeof(line), "%d %s, %s", daysRead, tr(STR_DAYS_READ), duration);
    renderer.drawCenteredText(UI_12_FONT_ID, y, line, true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing * 4;

    // Monthly totals, tallest month in black.
    const uint32_t maxMonth = std::max<uint32_t>(1, *std::max_element(monthSeconds, monthSeconds + 12));
    const int chartHeight = 130;
    const int slot = GRID_WIDTH / 12;
    for (int m = 0; m < 12; ++m) {
      const int h = static_cast<int>(static_cast<uint64_t>(monthSeconds[m]) * chartHeight / maxMonth);
      const int x = left + m * slot;
      if (monthSeconds[m] == maxMonth) {
        renderer.fillRect(x, y + chartHeight - h, slot - 8, h, true);
      } else {
        renderer.fillRectDither(x, y + chartHeight - h, slot - 8, h, DarkGray);
      }
      const char letter[2] = {MONTH_INITIALS[m], '\0'};
      renderer.drawText(UI_10_FONT_ID, x + (slot - 8 - renderer.getTextWidth(UI_10_FONT_ID, letter)) / 2,
                        y + chartHeight + 6, letter);
    }
    renderer.drawLine(left, y + chartHeight, left + GRID_WIDTH, y + chartHeight, true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", year.valid && year.year > year.firstYear ? tr(STR_EARLIER) : "",
                                            year.valid && year.year < year.lastYear ? tr(STR_LATER) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
