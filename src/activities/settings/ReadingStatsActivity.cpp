#include "ReadingStatsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Memory.h>
#include <TrustedTime.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "ReadingYearActivity.h"
#include "util/ReadingActivityView.h"

namespace {
constexpr size_t WEEKS_SHOWN = 12;
constexpr const char* WEEKDAYS[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

uint32_t secondsInHours(const readinglog::Summary& s, const int from, const int to) {
  uint32_t total = 0;
  for (int h = from; h < to; ++h) total += s.hourSeconds[h];
  return total;
}
}  // namespace

void ReadingStatsActivity::onEnter() {
  Activity::onEnter();
  readinglog::load(trustedtime::trustedNow(), summary);
  requestUpdate();
}

void ReadingStatsActivity::loop() {
  int x = 0;
  int y = 0;
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasScreenTapped(x, y)) {
    finish();
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) && summary.valid) {
    if (auto activity = makeUniqueNoThrow<ReadingYearActivity>(renderer, mappedInput)) {
      startActivityForResult(std::move(activity), nullptr);
    }
  }
}

void ReadingStatsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int left = metrics.contentSidePadding;
  const int right = width - metrics.contentSidePadding;
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, tr(STR_READING_ACTIVITY));
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;

  if (!summary.valid) {
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2 - 10, tr(STR_NO_READING_YET));
  } else {
    // Hours per week, oldest on the left; the current week is solid.
    renderer.drawText(UI_10_FONT_ID, left, y, tr(STR_LAST_12_WEEKS));
    y += renderer.getLineHeight(UI_10_FONT_ID) + metrics.verticalSpacing;
    uint32_t weekSeconds[WEEKS_SHOWN] = {};
    uint32_t maxWeek = 1;
    for (size_t i = 0; i < WEEKS_SHOWN; ++i) {
      const size_t week = readinglog::HISTORY_WEEKS - WEEKS_SHOWN + i;
      for (size_t d = 0; d < 7; ++d) weekSeconds[i] += summary.seconds[week * 7 + d];
      maxWeek = std::max(maxWeek, weekSeconds[i]);
    }
    const int chartHeight = 140;
    const int slot = (right - left) / static_cast<int>(WEEKS_SHOWN);
    for (size_t i = 0; i < WEEKS_SHOWN; ++i) {
      const int h = static_cast<int>(static_cast<uint64_t>(weekSeconds[i]) * chartHeight / maxWeek);
      const int x = left + static_cast<int>(i) * slot;
      if (i + 1 == WEEKS_SHOWN) {
        renderer.fillRect(x, y + chartHeight - h, slot - 6, h, true);
      } else {
        renderer.fillRectDither(x, y + chartHeight - h, slot - 6, h, DarkGray);
      }
    }
    renderer.drawLine(left, y + chartHeight, right, y + chartHeight, true);
    y += chartHeight + metrics.verticalSpacing * 3;

    // When you read: four parts of the day, longest bar in black.
    renderer.drawText(UI_10_FONT_ID, left, y, tr(STR_WHEN_YOU_READ));
    y += renderer.getLineHeight(UI_10_FONT_ID) + metrics.verticalSpacing;
    const StrId names[4] = {StrId::STR_MORNING, StrId::STR_AFTERNOON, StrId::STR_EVENING, StrId::STR_NIGHT};
    const uint32_t parts[4] = {secondsInHours(summary, 5, 12), secondsInHours(summary, 12, 17),
                               secondsInHours(summary, 17, 22),
                               secondsInHours(summary, 22, 24) + secondsInHours(summary, 0, 5)};
    const uint32_t maxPart = std::max<uint32_t>(1, *std::max_element(parts, parts + 4));
    const int labelWidth = 150;
    for (int i = 0; i < 4; ++i) {
      renderer.drawText(UI_10_FONT_ID, left, y, I18N.get(names[i]));
      const int barWidth = static_cast<int>(static_cast<uint64_t>(parts[i]) * (right - left - labelWidth) / maxPart);
      if (parts[i] == maxPart) {
        renderer.fillRect(left + labelWidth, y, barWidth, 22, true);
      } else {
        renderer.fillRectDither(left + labelWidth, y, barWidth, 22, DarkGray);
      }
      y += 42;
    }
    y += metrics.verticalSpacing * 2;
    renderer.drawLine(left, y, right, y, true);
    y += metrics.verticalSpacing * 2;

    // Plain facts, label on the left and value on the right.
    const auto fact = [&](const char* label, const char* value) {
      renderer.drawText(UI_10_FONT_ID, left, y, label);
      renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, value, EpdFontFamily::BOLD), y,
                        value, true, EpdFontFamily::BOLD);
      y += renderer.getLineHeight(UI_10_FONT_ID) + metrics.verticalSpacing;
    };
    char value[40];
    if (summary.sessions > 0) {
      readingview::formatDuration(summary.totalSeconds / summary.sessions, value, sizeof(value));
      fact(tr(STR_AVERAGE_SESSION), value);
    }
    uint32_t perWeekday[7] = {};
    uint32_t monthSeconds = 0;
    int todayYear, todayMonth, todayDay;
    readinglog::civilFromDay(summary.today, todayYear, todayMonth, todayDay);
    for (uint32_t day = summary.startDay; day <= summary.today; ++day) {
      const uint32_t s = summary.seconds[day - summary.startDay];
      perWeekday[readinglog::weekdayOfDay(day)] += s;
      int year, month, dayOfMonth;
      readinglog::civilFromDay(day, year, month, dayOfMonth);
      if (year == todayYear && month == todayMonth) monthSeconds += s;
    }
    const uint32_t* best = std::max_element(perWeekday, perWeekday + 7);
    if (*best > 0) fact(tr(STR_BEST_DAY), WEEKDAYS[best - perWeekday]);
    readingview::formatDuration(monthSeconds, value, sizeof(value));
    fact(tr(STR_THIS_MONTH), value);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), summary.valid ? tr(STR_YEAR) : "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
