#include "ReadingActivityView.h"

#include <GfxRenderer.h>

#include <cstdio>

#include "fontIds.h"

namespace readingview {
namespace {
constexpr const char* MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
constexpr uint32_t MIN_PEAK_DATA_SECONDS = 3 * 3600;

int hour12(const int hour) { return hour % 12 == 0 ? 12 : hour % 12; }
}  // namespace

void formatDuration(const uint32_t seconds, char* buf, const size_t size) {
  const uint32_t minutes = (seconds + 30) / 60;
  if (minutes < 60) {
    snprintf(buf, size, "%lu min", static_cast<unsigned long>(minutes));
  } else if (minutes % 60 == 0) {
    snprintf(buf, size, "%lu h", static_cast<unsigned long>(minutes / 60));
  } else {
    snprintf(buf, size, "%lu h %lu min", static_cast<unsigned long>(minutes / 60),
             static_cast<unsigned long>(minutes % 60));
  }
}

void formatPeakHours(const readinglog::Summary& summary, char* buf, const size_t size) {
  buf[0] = '\0';
  uint32_t total = 0;
  for (const uint32_t s : summary.hourSeconds) total += s;
  if (total < MIN_PEAK_DATA_SECONDS) return;
  int best = 0;
  uint32_t bestSeconds = 0;
  for (int h = 0; h < 24; ++h) {
    const uint32_t window = summary.hourSeconds[h] + summary.hourSeconds[(h + 1) % 24];
    if (window > bestSeconds) {
      bestSeconds = window;
      best = h;
    }
  }
  const int end = (best + 2) % 24;
  const bool sameHalf = (best < 12) == (end < 12) && end != 0;
  if (sameHalf) {
    snprintf(buf, size, "%d-%d %s", hour12(best), hour12(end), best < 12 ? "am" : "pm");
  } else {
    snprintf(buf, size, "%d %s-%d %s", hour12(best), best < 12 ? "am" : "pm", hour12(end), end < 12 ? "am" : "pm");
  }
}

void drawDayCell(const GfxRenderer& renderer, const int x, const int y, const int size, const uint8_t level) {
  switch (level) {
    case 1:
      renderer.fillRectDither(x, y, size, size, LightGray);
      break;
    case 2:
      renderer.fillRectDither(x, y, size, size, DarkGray);
      break;
    case 3:
      renderer.fillRect(x, y, size, size, true);
      break;
    default:
      break;
  }
  renderer.drawRect(x, y, size, size, true);
}

void drawGrid(const GfxRenderer& renderer, const int left, const int top, const readinglog::Summary& summary) {
  int lastLabelRight = left - 1000;
  for (size_t week = 0; week < readinglog::HISTORY_WEEKS; ++week) {
    const int x = left + static_cast<int>(week) * GRID_CELL;
    for (int row = 0; row < 7; ++row) {
      const uint32_t day = summary.startDay + static_cast<uint32_t>(week) * 7 + static_cast<uint32_t>(row);
      if (day > summary.today) continue;  // the rest of this week has not happened
      const int y = top + row * GRID_CELL;
      const int size = GRID_CELL - 3;
      drawDayCell(renderer, x, y, size, readinglog::level(summary.seconds[day - summary.startDay]));
      if (day == summary.today) renderer.drawRect(x - 2, y - 2, size + 4, size + 4, true);
    }
    // A month name above the first column that holds the 1st..7th of a month.
    int year, month, dayOfMonth;
    readinglog::civilFromDay(summary.startDay + static_cast<uint32_t>(week) * 7 + 6, year, month, dayOfMonth);
    if (dayOfMonth <= 7 && x > lastLabelRight) {
      const char* name = MONTHS[month - 1];
      renderer.drawText(SMALL_FONT_ID, x, top - 24, name);
      lastLabelRight = x + renderer.getTextWidth(SMALL_FONT_ID, name) + 6;
    }
  }
}

uint32_t lastSevenDays(const readinglog::Summary& summary, int& daysRead) {
  daysRead = 0;
  uint32_t total = 0;
  for (uint32_t i = 0; i < 7; ++i) {
    if (summary.today < summary.startDay + i) break;
    const uint16_t s = summary.seconds[summary.today - i - summary.startDay];
    total += s;
    if (s >= readinglog::MIN_READ_DAY_SECONDS) ++daysRead;
  }
  return total;
}

}  // namespace readingview
