#pragma once

#include <cstddef>
#include <cstdint>

#include "ReadingLog.h"

class GfxRenderer;

// Drawing helpers shared by the Reading Activity sleep screen and stats page.
namespace readingview {

constexpr int GRID_CELL = 25;
constexpr int GRID_WIDTH = static_cast<int>(readinglog::HISTORY_WEEKS) * GRID_CELL;
constexpr int GRID_HEIGHT = 7 * GRID_CELL;

// "23 min", "1 h", "2 h 10 min".
void formatDuration(uint32_t seconds, char* buf, size_t size);
// "9-11 pm" for the busiest two-hour window, empty when there is too little data.
void formatPeakHours(const readinglog::Summary& summary, char* buf, size_t size);

// One day: shaded by level (0 blank, 1 light, 2 medium, 3 black) with an outline.
void drawDayCell(const GfxRenderer& renderer, int x, int y, int size, uint8_t level);

// 18 weeks, one column per week (Monday on top), shaded by the day's minutes.
// Month names sit above the columns that start a month.
void drawGrid(const GfxRenderer& renderer, int left, int top, const readinglog::Summary& summary);

// Seconds read over the 7 days ending today, and how many of those days count as read.
uint32_t lastSevenDays(const readinglog::Summary& summary, int& daysRead);

}  // namespace readingview
