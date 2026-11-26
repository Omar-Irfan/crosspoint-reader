#include "ReadingActivityView.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "fontIds.h"

namespace readingview {
namespace {
constexpr const char* MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
constexpr const char* WEEKDAYS[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
constexpr const char* DAY_LETTERS[] = {"M", "T", "W", "T", "F", "S", "S"};
constexpr uint32_t MIN_PEAK_DATA_SECONDS = 3 * 3600;
constexpr float DEG = 3.14159265f / 180.0f;
// ponytail: weekday, day-letter, dial and bar labels are English, like MONTHS; translate if a UI language needs it.

int hour12(const int hour) { return hour % 12 == 0 ? 12 : hour % 12; }

// Start hour of the busiest two-hour window, or -1 with too little data.
int peakHour(const readinglog::Summary& summary) {
  uint32_t total = 0;
  for (const uint32_t s : summary.hourSeconds) total += s;
  if (total < MIN_PEAK_DATA_SECONDS) return -1;
  int best = 0;
  uint32_t bestSeconds = 0;
  for (int h = 0; h < 24; ++h) {
    const uint32_t window = summary.hourSeconds[h] + summary.hourSeconds[(h + 1) % 24];
    if (window > bestSeconds) {
      bestSeconds = window;
      best = h;
    }
  }
  return best;
}

uint16_t secondsOn(const readinglog::Summary& summary, const uint32_t day) {
  if (day < summary.startDay || day > summary.today) return 0;
  return summary.seconds[day - summary.startDay];
}

// A point on the dial: degrees clockwise from the top.
void dialPoint(const int cx, const int cy, const int radius, const float degrees, int& x, int& y) {
  x = cx + static_cast<int>(std::lround(radius * std::sin(degrees * DEG)));
  y = cy - static_cast<int>(std::lround(radius * std::cos(degrees * DEG)));
}

void drawCircle(const GfxRenderer& renderer, const int cx, const int cy, const int radius, const bool dotted) {
  int px = 0;
  int py = 0;
  dialPoint(cx, cy, radius, 0, px, py);
  for (int d = 4; d <= 360; d += 4) {
    int x = 0;
    int y = 0;
    dialPoint(cx, cy, radius, static_cast<float>(d), x, y);
    if (dotted) {
      renderer.drawPixel(x, y, true);
    } else {
      renderer.drawLine(px, py, x, y, true);
    }
    px = x;
    py = y;
  }
}

// "25m" or "1h35", short enough to sit above a day's bar.
void formatShortMinutes(const uint32_t seconds, char* buf, const size_t size) {
  const uint32_t minutes = (seconds + 30) / 60;
  if (minutes < 60) {
    snprintf(buf, size, "%lum", static_cast<unsigned long>(minutes));
  } else {
    snprintf(buf, size, "%luh%02lu", static_cast<unsigned long>(minutes / 60),
             static_cast<unsigned long>(minutes % 60));
  }
}
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
  const int best = peakHour(summary);
  if (best < 0) return;
  const int end = (best + 2) % 24;
  const bool sameHalf = (best < 12) == (end < 12) && end != 0;
  if (sameHalf) {
    snprintf(buf, size, "%d-%d %s", hour12(best), hour12(end), best < 12 ? "am" : "pm");
  } else {
    snprintf(buf, size, "%d %s-%d %s", hour12(best), best < 12 ? "am" : "pm", hour12(end), end < 12 ? "am" : "pm");
  }
}

const char* readerType(const readinglog::Summary& summary) {
  const int start = peakHour(summary);
  if (start < 0) return nullptr;
  const int middle = (start + 1) % 24;
  if (middle >= 4 && middle < 11) return tr(STR_EARLY_BIRD);
  if (middle >= 11 && middle < 17) return tr(STR_DAYTIME_READER);
  if (middle >= 17 && middle < 22) return tr(STR_EVENING_READER);
  return tr(STR_NIGHT_OWL);
}

uint32_t thisWeekSeconds(const readinglog::Summary& summary) {
  uint32_t total = 0;
  for (uint32_t day = summary.today - readinglog::weekdayOfDay(summary.today); day <= summary.today; ++day) {
    total += secondsOn(summary, day);
  }
  return total;
}

void formatDailyFact(const readinglog::Summary& summary, char* buf, const size_t size) {
  buf[0] = '\0';
  char value[32];
  enum Fact { BEST_WEEK, LONGEST_DAY, VERSUS_LAST_WEEK, DAYS_OF_30, THIS_MONTH, AVERAGE_SESSION, FACT_COUNT };
  // Start from today's fact and take the first one there is data for.
  for (int i = 0; i < FACT_COUNT && buf[0] == '\0'; ++i) {
    switch ((summary.today + i) % FACT_COUNT) {
      case BEST_WEEK: {
        uint32_t best = 0;
        for (size_t week = 0; week < readinglog::HISTORY_WEEKS; ++week) {
          uint32_t total = 0;
          for (size_t d = 0; d < 7; ++d) total += summary.seconds[week * 7 + d];
          best = std::max(best, total);
        }
        if (best < readinglog::MIN_READ_DAY_SECONDS) break;
        formatDuration(best, value, sizeof(value));
        snprintf(buf, size, "%s: %s", tr(STR_BEST_WEEK), value);
        break;
      }
      case LONGEST_DAY: {
        uint32_t bestDay = 0;
        uint16_t bestSeconds = 0;
        for (uint32_t day = summary.startDay; day <= summary.today; ++day) {
          if (secondsOn(summary, day) > bestSeconds) {
            bestSeconds = secondsOn(summary, day);
            bestDay = day;
          }
        }
        if (bestSeconds < readinglog::MIN_READ_DAY_SECONDS) break;
        int year, month, dayOfMonth;
        readinglog::civilFromDay(bestDay, year, month, dayOfMonth);
        formatDuration(bestSeconds, value, sizeof(value));
        snprintf(buf, size, "%s: %s %d %s, %s", tr(STR_LONGEST_DAY), WEEKDAYS[readinglog::weekdayOfDay(bestDay)],
                 dayOfMonth, MONTHS[month - 1], value);
        break;
      }
      case VERSUS_LAST_WEEK: {
        // This week so far against the same days of last week.
        const uint32_t monday = summary.today - readinglog::weekdayOfDay(summary.today);
        uint32_t lastWeek = 0;
        for (uint32_t day = monday - 7; day <= summary.today - 7; ++day) lastWeek += secondsOn(summary, day);
        const uint32_t thisWeek = thisWeekSeconds(summary);
        const uint32_t difference = thisWeek > lastWeek ? thisWeek - lastWeek : lastWeek - thisWeek;
        if (lastWeek == 0 || (difference + 30) / 60 == 0) break;
        formatDuration(difference, value, sizeof(value));
        snprintf(buf, size, thisWeek > lastWeek ? tr(STR_AHEAD_OF_LAST_WEEK_FORMAT) : tr(STR_BEHIND_LAST_WEEK_FORMAT),
                 value);
        break;
      }
      case DAYS_OF_30: {
        int read = 0;
        for (uint32_t back = 0; back < 30 && summary.today >= back; ++back) {
          read += secondsOn(summary, summary.today - back) >= readinglog::MIN_READ_DAY_SECONDS;
        }
        if (read == 0) break;
        snprintf(buf, size, tr(STR_READ_ON_DAYS_FORMAT), read);
        break;
      }
      case THIS_MONTH: {
        int year, month, dayOfMonth;
        readinglog::civilFromDay(summary.today, year, month, dayOfMonth);
        uint32_t total = 0;
        for (uint32_t day = summary.today + 1 - static_cast<uint32_t>(dayOfMonth); day <= summary.today; ++day) {
          total += secondsOn(summary, day);
        }
        if (total < readinglog::MIN_READ_DAY_SECONDS) break;
        formatDuration(total, value, sizeof(value));
        snprintf(buf, size, "%s: %s", tr(STR_THIS_MONTH), value);
        break;
      }
      case AVERAGE_SESSION: {
        if (summary.sessions == 0) break;
        formatDuration(summary.totalSeconds / summary.sessions, value, sizeof(value));
        snprintf(buf, size, "%s: %s", tr(STR_AVERAGE_SESSION), value);
        break;
      }
      default:
        break;
    }
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

void drawGrid(const GfxRenderer& renderer, const int left, const int top, const readinglog::Summary& summary,
              const int cell) {
  const uint16_t typical = readinglog::typicalDaySeconds(summary.seconds, summary.today - summary.startDay + 1);
  int lastLabelRight = left - 1000;
  for (size_t week = 0; week < readinglog::HISTORY_WEEKS; ++week) {
    const int x = left + static_cast<int>(week) * cell;
    for (int row = 0; row < 7; ++row) {
      const uint32_t day = summary.startDay + static_cast<uint32_t>(week) * 7 + static_cast<uint32_t>(row);
      if (day > summary.today) continue;  // the rest of this week has not happened
      const int y = top + row * cell;
      const int size = cell - 3;
      drawDayCell(renderer, x, y, size, readinglog::level(summary.seconds[day - summary.startDay], typical));
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

void drawShadeKey(const GfxRenderer& renderer, const int right, const int y, const int cell) {
  const int size = cell - 6;
  const char* more = tr(STR_MORE);
  const char* less = tr(STR_LESS);
  int x = right - renderer.getTextWidth(SMALL_FONT_ID, more);
  renderer.drawText(SMALL_FONT_ID, x, y, more);
  const int cellTop = y + (renderer.getLineHeight(SMALL_FONT_ID) - size) / 2;
  for (int level = 3; level >= 0; --level) {
    x -= size + 5;
    drawDayCell(renderer, x, cellTop, size, static_cast<uint8_t>(level));
  }
  x -= renderer.getTextWidth(SMALL_FONT_ID, less) + 6;
  renderer.drawText(SMALL_FONT_ID, x, y, less);
}

void drawHourDial(const GfxRenderer& renderer, const int cx, const int cy, const int innerRadius, const int outerRadius,
                  const readinglog::Summary& summary) {
  drawCircle(renderer, cx, cy, outerRadius + 3, true);
  drawCircle(renderer, cx, cy, innerRadius - 4, false);

  const uint32_t maxHour = std::max<uint32_t>(1, *std::max_element(summary.hourSeconds, summary.hourSeconds + 24));
  const int reach = outerRadius - innerRadius;
  for (int h = 0; h < 24; ++h) {
    const int length = static_cast<int>(static_cast<uint64_t>(summary.hourSeconds[h]) * reach / maxHour);
    const float from = h * 15.0f + 2.0f;
    const float to = h * 15.0f + 13.0f;
    if (length < 2) {
      int x = 0;
      int y = 0;
      dialPoint(cx, cy, innerRadius, h * 15.0f + 7.5f, x, y);
      renderer.fillRect(x - 1, y - 1, 3, 3, true);
      continue;
    }
    int xs[5];
    int ys[5];
    dialPoint(cx, cy, innerRadius, from, xs[0], ys[0]);
    dialPoint(cx, cy, innerRadius + length, from, xs[1], ys[1]);
    dialPoint(cx, cy, innerRadius + length, h * 15.0f + 7.5f, xs[2], ys[2]);
    dialPoint(cx, cy, innerRadius + length, to, xs[3], ys[3]);
    dialPoint(cx, cy, innerRadius, to, xs[4], ys[4]);
    renderer.fillPolygon(xs, ys, 5, true);
  }

  // Midnight on top, noon at the bottom.
  const int lineHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int labelRadius = outerRadius + 8;
  renderer.drawText(SMALL_FONT_ID, cx - renderer.getTextWidth(SMALL_FONT_ID, "12am") / 2, cy - labelRadius - lineHeight,
                    "12am");
  renderer.drawText(SMALL_FONT_ID, cx - renderer.getTextWidth(SMALL_FONT_ID, "12pm") / 2, cy + labelRadius, "12pm");
  renderer.drawText(SMALL_FONT_ID, cx + labelRadius, cy - lineHeight / 2, "6am");
  renderer.drawText(SMALL_FONT_ID, cx - labelRadius - renderer.getTextWidth(SMALL_FONT_ID, "6pm"), cy - lineHeight / 2,
                    "6pm");
}

void drawWeekBars(const GfxRenderer& renderer, const int left, const int top, const int width, const int barHeight,
                  const readinglog::Summary& summary) {
  const uint32_t monday = summary.today - readinglog::weekdayOfDay(summary.today);
  uint32_t maxDay = 30 * 60;  // a short week still draws short bars
  for (uint32_t d = 0; d < 7; ++d) maxDay = std::max<uint32_t>(maxDay, secondsOn(summary, monday + d));

  const int slot = width / 7;
  const int barWidth = slot * 3 / 5;
  const int labelHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int baseline = top + labelHeight + barHeight;
  char label[16];
  for (uint32_t d = 0; d < 7; ++d) {
    const uint32_t day = monday + d;
    const int x = left + static_cast<int>(d) * slot + (slot - barWidth) / 2;
    const int centre = x + barWidth / 2;
    if (day <= summary.today) {
      const uint16_t seconds = secondsOn(summary, day);
      const int h = static_cast<int>(static_cast<uint64_t>(seconds) * barHeight / maxDay);
      if (h <= 0) {
        // nothing read: the baseline alone
      } else if (day == summary.today) {
        renderer.fillRect(x, baseline - h, barWidth, h, true);
      } else {
        renderer.fillRectDither(x, baseline - h, barWidth, h, DarkGray);
        renderer.drawRect(x, baseline - h, barWidth, h, true);
      }
      if (seconds >= 60) {
        formatShortMinutes(seconds, label, sizeof(label));
        renderer.drawText(SMALL_FONT_ID, centre - renderer.getTextWidth(SMALL_FONT_ID, label) / 2,
                          baseline - h - labelHeight - 2, label);
      }
    }
    const EpdFontFamily::Style style = day == summary.today ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    renderer.drawText(UI_10_FONT_ID, centre - renderer.getTextWidth(UI_10_FONT_ID, DAY_LETTERS[d], style) / 2,
                      baseline + 6, DAY_LETTERS[d], true, style);
  }
  renderer.drawLine(left, baseline, left + width, baseline, true);
}

}  // namespace readingview
